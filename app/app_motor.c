#include "app_motor.h"
#ifdef FEEDER_QUICK_TIMED
#include "quick_timed_config.h"
static void quick_finish(AppMotor *a,uint32_t now,uint32_t completed)
{
    if (!a->quick_active) { return; }
    a->quick_active=0U;a->quick_last_end=now;a->quick_has_end=1U;a->dirty=1U;
    if (completed) { a->quick_completed++; } else { a->quick_canceled++; }
}
void app_motor_timing(AppMotor *a,uint32_t now)
{
    if (!a->quick_active) { return; }
    /* 检查先于主循环写GPIO；停止/故障取消，绝不恢复先前的运行请求。 */
    if (!a->motor->motor_on || a->motor->fault) { quick_finish(a,now,0U); }
    else if ((uint32_t)(now-a->quick_started)>=FEEDER_QUICK_DURATION_MS) {
        touch_motor_stop(a->motor);quick_finish(a,now,1U);
    }
}
#endif
static int receive(void *context,const uint8_t *data,uint32_t length,uint32_t now)
{
    AppMotor *a=context;TouchMotor *s=a->motor;
    uint32_t offset=0U,mask=0U,values=0U,legacy=0U;
    if (!length) { return 0; }
    /* 先验证整帧再动作；后半帧损坏、未知DP或冲突值均不得先启动。 */
    while (offset<length) {
        if (length-offset<4U) { a->rejected++;return 0; }
        uint32_t id=data[offset],type=data[offset+1U];
        uint32_t size=((uint32_t)data[offset+2U]<<8U)|data[offset+3U];offset+=4U;
        if (size>length-offset) { a->rejected++;return 0; }
        if ((id>=101U && id<=103U)
#ifdef FEEDER_QUICK_TIMED
            || id==2U
#endif
        ) {
            if (type!=1U || size!=1U || data[offset]>1U) { a->rejected++;return 0; }
#ifdef FEEDER_QUICK_TIMED
            uint32_t bit=(id==2U)?8U:(1U<<(id-101U)),value=data[offset]?bit:0U;
#else
            uint32_t bit=1U<<(id-101U),value=data[offset]?bit:0U;
#endif
            if ((mask&bit) && ((values&bit)!=value)) { a->rejected++;return 0; }
            mask|=bit;values=(values&~bit)|value;
        } else { legacy=1U; }
        offset+=size;
    }
    if (!mask) { return -1; } /* 标准喂食DP仍由原解析器校验/拦截。 */
    if (legacy) { a->rejected++;return 0; } /* 不混合接受未知/喂食与执行器指令。 */
    a->received++;a->dirty=1U;
#ifdef FEEDER_INPUT_DIAG
    /* 诊断仅观察输入，任何合法App动作也不启动；回报真实停止。 */
    (void)now;touch_motor_stop(s);a->rejected++;return 1;
#endif
    /* 102=true及101=false均要求停止，优先于同帧开启。
     * 102=false仅为停止按钮复位值，不反向触发启动。 */
    uint32_t stop=((mask&2U)&&(values&2U)) || ((mask&1U)&&!(values&1U));
    if (stop) { touch_motor_stop(s); }
#ifdef FEEDER_QUICK_TIMED
    if (stop) { quick_finish(a,now,0U); }
#endif
    if (s->phase==TOUCH_BOOT || s->phase==TOUCH_FAULT) { a->rejected++;return 1; }
    if ((mask&4U) && s->direction!=((values>>2U)&1U)) {
        s->direction=(values>>2U)&1U;s->reversals++;
        if (s->motor_on) { s->pending=1U;s->switch_ms=now; }
    }
#ifdef FEEDER_QUICK_TIMED
    uint32_t quick=(mask&8U)&&(values&8U);
    if (!stop && quick) {
        /* 运行中不叠加、不重新计时；1秒冷却避免紧跟完成的重复帧立即再启动。
         * 无请求ID的Bool无法区分所有延迟重发和新点击，不宣称端到端恰好一次。 */
        if (!s->motor_on && !s->pending && !a->quick_active &&
            (!a->quick_has_end || (uint32_t)(now-a->quick_last_end)>=FEEDER_QUICK_COOLDOWN_MS)) {
            a->quick_active=1U;a->quick_started=now;s->motor_on=1U;
        } else { a->quick_ignored++; }
    }
    /* 正在定时测试时101=true也不能把它变成无限运行；先停止再开可持续运行。 */
    if (!stop && !quick && !a->quick_active && (mask&1U) && (values&1U)) { s->motor_on=1U; }
#else
    if (!stop && (mask&1U) && (values&1U)) { s->motor_on=1U; }
#endif
    /* 清除尚未提交的触摸短按，防止App停止后残留手势产生额外换向。 */
    s->short_pending=0U;
    return 1;
}
void app_motor_bind(AppMotor *a,TouchMotor *s,WbrInit *link)
{
    *a=(AppMotor){.motor=s,.link=link,.dirty=1U,.reported_state=UINT32_MAX};
    link->app_dp=receive;link->app_context=a;
}
void app_motor_poll(AppMotor *a,uint32_t now)
{
    TouchMotor *s=a->motor;WbrInit *link=a->link;
    uint32_t state=s->motor_on|(s->direction<<1U)|(s->pending<<2U)|(s->fault?8U:0U);
#ifdef FEEDER_QUICK_TIMED
    state|=a->quick_active<<4U;
#endif
    if (state!=a->reported_state || link->app_report_request) { a->dirty=1U; }
    if (!a->dirty || (uint32_t)(now-a->last_report)<100U) { return; }
    /* 101回报运行请求，102自动复位false，103回报所选方向。
     * DP4仅反映当前驱动输出：待机0/驱动1，绝不上报已完成一圈或出粮完成2。 */
    uint8_t report[]={101U,1U,0U,1U,(uint8_t)s->motor_on,
                      102U,1U,0U,1U,0U,103U,1U,0U,1U,(uint8_t)s->direction,
                      4U,4U,0U,1U,(uint8_t)(s->motor_on && !s->pending)
#ifdef FEEDER_QUICK_TIMED
                      ,2U,1U,0U,1U,(uint8_t)a->quick_active
#endif
    };
    if (wbr_init_app_report(link,report,sizeof(report))) {
        a->reported_state=state;a->dirty=0U;a->last_report=now;link->app_report_request=0U;
    }
}

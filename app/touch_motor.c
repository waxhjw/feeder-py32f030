#include "touch_motor.h"
#include "touch_motor_config.h"
void touch_motor_reset(TouchMotor *s, uint32_t now)
{
    *s = (TouchMotor){.phase=TOUCH_BOOT,.boot_ms=now,.last_ms=now,.raw=1U};
}
void touch_motor_fail(TouchMotor *s, uint32_t fault)
{
    s->motor_on=0U; s->fault=fault; s->phase=TOUCH_FAULT; s->held_ms=0U;
#ifdef FEEDER_TOUCH_REVERSE
    s->pending=0U; s->short_pending=0U;
#endif
}
void touch_motor_stop(TouchMotor *s)
{
    s->motor_on=0U; s->held_ms=0U; s->releasing=0U;
#ifdef FEEDER_TOUCH_REVERSE
    s->pending=0U; s->short_pending=0U;
#endif
    if (s->phase!=TOUCH_FAULT && s->phase!=TOUCH_BOOT) { s->phase=TOUCH_RELEASE; }
}
void touch_motor_step(TouchMotor *s, uint32_t now, uint32_t level)
{
    if (s->phase==TOUCH_FAULT) { return; }
    if (level>1U) { touch_motor_fail(s,1U); return; }
    if ((uint32_t)(now-s->last_ms)>TOP1_MAX_SAMPLE_GAP_MS) { touch_motor_fail(s,2U); return; }
    s->last_ms=now; s->raw=level;
#ifdef FEEDER_TOUCH_REVERSE
    /* 换向等待时持续撤销驱动；时间到且没有STOP/故障才恢复。
     * 300ms是验证版暂停参数，不代表已检测转子停稳。 */
    if (s->pending && (uint32_t)(now-s->switch_ms)>=TOP1_REVERSE_PAUSE_MS) { s->pending=0U; }
#endif
    uint32_t pressed=(level==TOP1_ACTIVE_LEVEL);
    if (s->phase==TOUCH_BOOT) {
        /* 开机两秒手应离开。输入必须在结束前连续松开500ms；
         * 电平配置反向、上电一直按住或输入异常时本次锁定，不自动猜极性。 */
        s->motor_on=0U;
        if (pressed) { s->releasing=0U; }
        else if (s->releasing==0U) { s->releasing=1U; s->release_ms=now; }
        if ((uint32_t)(now-s->boot_ms)>=TOP1_BOOT_MS) {
            if (s->releasing && (uint32_t)(now-s->release_ms)>=TOP1_BOOT_RELEASE_MS) { s->phase=TOUCH_READY; }
            else { touch_motor_fail(s,3U); }
        }
        return;
    }
    if (s->phase==TOUCH_RELEASE) {
        if (pressed) {
            s->releasing=0U;
#ifdef FEEDER_TOUCH_REVERSE
            s->short_pending=0U; /* 松手未稳定又触摸，本次短按不提交。 */
#endif
        }
        else {
            if (s->releasing==0U) { s->releasing=1U; s->release_ms=now; }
            if ((uint32_t)(now-s->release_ms)>=TOP1_RELEASE_MS) {
#ifdef FEEDER_TOUCH_REVERSE
                if (s->short_pending) {
                    s->short_pending=0U; s->direction^=1U; s->reversals++;
                    if (s->motor_on) { s->pending=1U; s->switch_ms=now; }
                }
#endif
                s->phase=TOUCH_READY; s->held_ms=0U;
            }
        }
        return;
    }
    if (s->phase==TOUCH_READY && pressed) { s->press_ms=now; s->phase=TOUCH_HOLD; }
    if (s->phase==TOUCH_HOLD) {
        if (pressed==0U) {
#ifdef FEEDER_TOUCH_REVERSE
            /* 释放时才知道是短按；连续松手100ms后提交一次。
             * 800ms到3秒之间松开不动作，满3秒已经走长按分支。 */
            uint32_t duration=(uint32_t)(now-s->press_ms);
            s->short_pending=(duration>=TOP1_SHORT_MIN_MS && duration<=TOP1_SHORT_MAX_MS);
            s->releasing=1U; s->release_ms=now; s->phase=TOUCH_RELEASE;
            s->held_ms=0U; return;
#else
            s->held_ms=0U; s->phase=TOUCH_READY; return;
#endif
        }
        s->held_ms=(uint32_t)(now-s->press_ms);
        if (s->held_ms>=TOP1_LONG_MS) {
            s->motor_on^=1U; s->toggles++; s->phase=TOUCH_RELEASE; s->releasing=0U;
#ifdef FEEDER_TOUCH_REVERSE
            s->pending=0U; s->short_pending=0U;
#endif
        }
    }
}

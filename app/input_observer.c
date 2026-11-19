#include "input_observer.h"
void input_observer_step(InputObserver *s,uint32_t now,uint32_t levels)
{
    uint32_t gap=s->started?(uint32_t)(now-s->last_ms):0U;
    if(s->started && !gap) { return; }
    if(gap>s->max_gap) { s->max_gap=gap; }
    if(gap>50U) { s->gaps++; }
    for(uint32_t i=0U;i<3U;i++) {
        InputChannel *c=&s->channel[i];uint32_t raw=(levels>>i)&1U;
        /* 开始观察或采样中断后重新建立基准，不把未知期间算作有效边沿。 */
        if(!s->started || gap>50U) { c->valid=0U;c->candidate=raw;c->since=now; }
        c->raw=raw;
        if(c->candidate!=raw) { c->candidate=raw;c->since=now; }
        if((uint32_t)(now-c->since)>=20U && (!c->valid || c->stable!=raw)) {
            if(c->valid) { if(raw) { c->rises++; } else { c->falls++; } }
            c->stable=raw;c->valid=1U;c->last_ms=now;
        }
    }
    s->started=1U;s->last_ms=now;s->samples++;
}

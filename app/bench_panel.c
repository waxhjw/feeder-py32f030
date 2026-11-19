#include "bench_panel.h"

uint32_t panel_keys_step(PanelKeys *keys,uint32_t now,uint32_t stable,uint32_t valid,uint32_t busy)
{
    if (!valid) { *keys=(PanelKeys){0};return PANEL_NO_KEY; }
    if (!keys->ready) { keys->ready=stable==0U;return PANEL_NO_KEY; }
    uint32_t held=keys->held, age=now-keys->at;
    if (!stable) {
        uint32_t evt=!busy && !keys->consumed && age>=40U && age<=800U && held<=2U?held:0U;
        keys->held=keys->consumed=0U;return evt;
    }
    if (stable!=held) {
        if (!held || (stable==3U && !keys->consumed && !busy)) {
            keys->at=now;keys->consumed=busy;
        } else { keys->consumed=1U; }
        keys->held=stable;
        return PANEL_NO_KEY;
    }
    if (busy) { keys->consumed=1U; }
    if (keys->consumed) { return PANEL_NO_KEY; }
    if ((held==3U && age>=2000U) || (held==2U && age>=5000U)) {
        keys->consumed=1U;return held==3U?PANEL_MUTE:PANEL_PAIR;
    }
    return PANEL_NO_KEY;
}

void panel_sound_start(PanelSound *sound,uint32_t now,uint32_t cue)
{
    sound->at=now;sound->cue=sound->muted?PANEL_SILENT:cue;
}
uint32_t panel_sound_output(const PanelSound *sound,uint32_t now)
{
    if (sound->muted || sound->cue==PANEL_SILENT) { return BUZZER_OFF; }
    uint32_t length=sound->cue==PANEL_CANCEL?220U:110U, t=now-sound->at;
    if (sound->cue==PANEL_COMPLETE && t>=190U) { t-=190U; }
    /* 无源蜂鸣器在提示窗口内使用PWM，窗口结束后关闭。 */
    return t<length?BUZZER_TONE:BUZZER_OFF;
}

void panel_time_frame(uint32_t valid,uint32_t minute,uint8_t frame[7])
{
    static const uint8_t seg[]={0x3FU,0x06U,0x5BU,0x4FU,0x66U,0x6DU,0x7DU,0x07U,0x7FU,0x6FU};
    minute%=1440U;
    uint32_t hour=minute/60U;minute%=60U;
    frame[0]=8U;frame[5]=frame[6]=0U;
    frame[1]=valid?seg[hour/10U]:0x40U;frame[2]=valid?seg[hour%10U]:0x40U;
    frame[3]=valid?seg[minute/10U]:0x40U;frame[4]=valid?seg[minute%10U]:0x40U;
}

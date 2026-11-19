#ifndef FEEDER_BENCH_PANEL_H
#define FEEDER_BENCH_PANEL_H
#include <stdint.h>
enum { PANEL_NO_KEY, PANEL_TOP1, PANEL_TOP2, PANEL_MUTE, PANEL_PAIR };
enum { PANEL_SILENT, PANEL_CONFIRM, PANEL_COMPLETE, PANEL_CANCEL };
enum { BUZZER_OFF, BUZZER_TONE };
typedef struct {
    uint32_t held, at, consumed, ready;
} PanelKeys;
typedef struct {
    uint32_t cue, at, muted;
} PanelSound;
/* stable：bit0为TOP1按下，bit1为TOP2按下。长按只事件一次，松手不再算短按。 */
uint32_t panel_keys_step(PanelKeys *keys,uint32_t now,uint32_t stable,uint32_t valid,uint32_t busy);
void panel_sound_start(PanelSound *sound,uint32_t now,uint32_t cue);
uint32_t panel_sound_output(const PanelSound *sound,uint32_t now);
void panel_time_frame(uint32_t valid,uint32_t minute,uint8_t frame[7]);
#endif

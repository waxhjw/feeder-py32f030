#ifndef FEEDER_TUYA_FRAME_H
#define FEEDER_TUYA_FRAME_H
#include <stdint.h>
#ifndef TUYA_PAYLOAD_LIMIT
#define TUYA_PAYLOAD_LIMIT 128U
#endif
/* 完整帧校验器，供需要应答的协议层使用。无发送或执行器接口。
 * feed返回1时，data中的报文仅保证在下次feed之前有效。
 * 正常数据帧的载荷中即使含有心跳字节，也不会被当作另一帧执行。
 */
typedef struct {
    uint32_t used, drain, frames, bad_checksum, too_long;
    uint8_t data[TUYA_PAYLOAD_LIMIT + 7U];
} TuyaFrameParser;
void tuya_frame_init(TuyaFrameParser *parser);
void tuya_frame_gap(TuyaFrameParser *parser);
int tuya_frame_feed(TuyaFrameParser *parser, uint8_t byte);
#endif

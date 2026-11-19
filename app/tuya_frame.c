#include "tuya_frame.h"
#include <string.h>

void tuya_frame_init(TuyaFrameParser *parser) { memset(parser, 0, sizeof(*parser)); }
void tuya_frame_gap(TuyaFrameParser *parser) { parser->used = parser->drain = 0U; }

int tuya_frame_feed(TuyaFrameParser *parser, uint8_t byte)
{
    if (parser->drain != 0U) { parser->drain--; return 0; }
    if (parser->used == 0U) {
        if (byte == 0x55U) { parser->data[parser->used++] = byte; }
        return 0;
    }
    if (parser->used == 1U && byte != 0xAAU) {
        parser->used = byte == 0x55U ? 1U : 0U;
        return 0;
    }
    parser->data[parser->used++] = byte;
    if (parser->used < 6U) { return 0; }
    uint32_t length = ((uint32_t)parser->data[4] << 8U) | parser->data[5];
    if (length > TUYA_PAYLOAD_LIMIT) {
        parser->too_long++;
        parser->used = 0U;
        parser->drain = length + 1U; /* 丢弃整个超长载荷及校验，不响应其内部伪心跳。 */
        return 0;
    }
    if (parser->used < length + 7U) { return 0; }
    uint8_t checksum = 0U;
    for (uint32_t i = 0U; i < length + 6U; ++i) { checksum += parser->data[i]; }
    parser->used = 0U;
    if (checksum != byte) { parser->bad_checksum++; return 0; }
    parser->frames++;
    return 1;
}

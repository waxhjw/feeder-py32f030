#ifndef FEEDER_LED_PATTERN_H
#define FEEDER_LED_PATTERN_H
#include <stdint.h>
#define LED_PATTERN_STEPS 41U
/* 一帧为地址0x08及六个GRID数据；未接的GRID6恒为零。 */
uint32_t led_pattern_frame(uint32_t step, uint8_t frame[7]);
/* 8位数据、低位先发、奇校验、1停止位，返回含起止位的11位。 */
uint16_t led_encode_byte(uint8_t byte);
#endif

#ifndef FEEDER_BOARD_CONFIG_H
#define FEEDER_BOARD_CONFIG_H

/* 当前主要依据：WIFI显示喂食器.PDF（2026-07-29图纸，第3页，20脚）。
 * 归档 hardware/source/schematic-wifi-display-20261107.pdf；旧图仅追溯。
 * 目标实物为 V1.2，连线与实装型号尚待硬件逐项确认。
 * 本阶段仅验证工具链；不得把本产物当作可上板的安全验证固件。
 */
#define FEEDER_FIRMWARE_VERSION "0.1.0-env"
#define FEEDER_TARGET_PART "PY32F030F18P6TU"
#define FEEDER_MOTION_ENABLED 0

/* 原理图导线标签 PA2 -> 实际 U1 PA8（第10脚），连接 U13 INA。
 * 原理图导线标签 PA3 -> 实际 U1 PA11（第13脚），连接 U13 INB。
 * 只记录映射，不配置输出；TC118S 真值表与板上停止电平待确认。
 */
#define FEEDER_MOTOR_INA_ACTUAL_PA_PIN 8U
#define FEEDER_MOTOR_INB_ACTUAL_PA_PIN 11U

#if FEEDER_MOTION_ENABLED
#error "Environment baseline must not enable motion"
#endif
#endif

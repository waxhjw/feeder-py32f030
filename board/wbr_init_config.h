#ifndef FEEDER_WBR_INIT_CONFIG_H
#define FEEDER_WBR_INIT_CONFIG_H
#include "wbr_rx_config.h"
/* 与0.4.0相同的板载USART2线路、8MHz内部时钟和电机关闭路径。
 * 主图第3页 / PY32数据手册V2.8表3-8 / 参考手册V1.7第409~414页。
 * PID来自本产品后台及20261103下载的专属SDK；不使用其他产品的PID。
 */
#ifdef FEEDER_CYCLE_NET
#if defined(FEEDER_CYCLE_SUITE)
#define FEEDER_WBR_INIT_VERSION "0.15.6-bench-suite"
#define FEEDER_WBR_INIT_ID 0x000F0600UL
#define FEEDER_TUYA_MCU_VERSION "0.15.6"
#elif defined(FEEDER_CYCLE_MANUAL_DP)
#define FEEDER_WBR_INIT_VERSION "0.15.5-cycle-manual-diag"
#define FEEDER_WBR_INIT_ID 0x000F0500UL
#define FEEDER_TUYA_MCU_VERSION "0.15.5"
#else
#define FEEDER_WBR_INIT_VERSION "0.15.4-cycle-net-diag"
#define FEEDER_WBR_INIT_ID 0x000F0400UL
#define FEEDER_TUYA_MCU_VERSION "0.15.4"
#endif
#define FEEDER_TUYA_CONFIG_MODE "0"
#elif defined(FEEDER_PAIR_MODE)
#ifdef FEEDER_TOUCH_MOTOR
#ifdef FEEDER_TOUCH_REVERSE
#ifdef FEEDER_APP_MOTOR
#ifdef FEEDER_QUICK_TIMED
#ifdef FEEDER_OTA_APP
#ifdef FEEDER_ADC_DIAG
#ifdef FEEDER_PLAN_DIAG
#ifdef FEEDER_LOCAL_UI
#ifdef FEEDER_PLAN_APP
#ifdef FEEDER_PLAN_APP_EMPTY
#ifdef FEEDER_PLAN_APP_TABLE
#define FEEDER_WBR_INIT_VERSION "0.15.2-ota-plan-app"
#define FEEDER_WBR_INIT_ID 0x000F0200UL
#define FEEDER_TUYA_MCU_VERSION "0.15.2"
#else
#define FEEDER_WBR_INIT_VERSION "0.15.1-ota-plan-app"
#define FEEDER_WBR_INIT_ID 0x000F0100UL
#define FEEDER_TUYA_MCU_VERSION "0.15.1"
#endif
#else
#define FEEDER_WBR_INIT_VERSION "0.15.0-ota-plan-app"
#define FEEDER_WBR_INIT_ID 0x000F0000UL
#define FEEDER_TUYA_MCU_VERSION "0.15.0"
#endif
#else
#define FEEDER_WBR_INIT_VERSION "0.14.0-ota-local-controls"
#define FEEDER_WBR_INIT_ID 0x000E0000UL
#define FEEDER_TUYA_MCU_VERSION "0.14.0"
#endif
#elif defined(FEEDER_STORE_CACHED)
#define FEEDER_WBR_INIT_VERSION "0.13.2-ota-plan-diag"
#define FEEDER_WBR_INIT_ID 0x000D0200UL
#define FEEDER_TUYA_MCU_VERSION "0.13.2"
#elif defined(FEEDER_STORE_PROFILE)
#define FEEDER_WBR_INIT_VERSION "0.13.1-ota-plan-diag"
#define FEEDER_WBR_INIT_ID 0x000D0100UL
#define FEEDER_TUYA_MCU_VERSION "0.13.1"
#else
#define FEEDER_WBR_INIT_VERSION "0.13.0-ota-plan-diag"
#define FEEDER_WBR_INIT_ID 0x000D0000UL
#define FEEDER_TUYA_MCU_VERSION "0.13.0"
#endif
#elif defined(FEEDER_ADC_REARM)
#define FEEDER_WBR_INIT_VERSION "0.12.3-ota-adc-diag"
#define FEEDER_WBR_INIT_ID 0x000C0300UL
#define FEEDER_TUYA_MCU_VERSION "0.12.3"
#elif defined(FEEDER_ADC_RS_FIX)
#define FEEDER_WBR_INIT_VERSION "0.12.2-ota-adc-diag"
#define FEEDER_WBR_INIT_ID 0x000C0200UL
#define FEEDER_TUYA_MCU_VERSION "0.12.2"
#else
#define FEEDER_WBR_INIT_VERSION "0.12.1-ota-adc-diag"
#define FEEDER_WBR_INIT_ID 0x000C0100UL
#define FEEDER_TUYA_MCU_VERSION "0.12.1"
#endif
#elif defined(FEEDER_INPUT_DIAG)
#define FEEDER_WBR_INIT_VERSION "0.12.0-ota-input-diag"
#define FEEDER_WBR_INIT_ID 0x000C0000UL
#define FEEDER_TUYA_MCU_VERSION "0.12.0"
#elif defined(FEEDER_OTA_NEXT)
#define FEEDER_WBR_INIT_VERSION "0.11.1-ota"
#define FEEDER_WBR_INIT_ID 0x000B0100UL
#define FEEDER_TUYA_MCU_VERSION "0.11.1"
#else
#define FEEDER_WBR_INIT_VERSION "0.11.0-ota"
#define FEEDER_WBR_INIT_ID 0x000B0000UL
#define FEEDER_TUYA_MCU_VERSION "0.11.0"
#endif
#else
#define FEEDER_WBR_INIT_VERSION "0.10.1-quick-timed"
#define FEEDER_WBR_INIT_ID 0x000A0100UL
#define FEEDER_TUYA_MCU_VERSION "0.10.1"
#endif
#else
#define FEEDER_WBR_INIT_VERSION "0.10.0-app-motor"
#define FEEDER_WBR_INIT_ID 0x000A0000UL
#define FEEDER_TUYA_MCU_VERSION "0.10.0"
#endif
#else
#define FEEDER_WBR_INIT_VERSION "0.9.1-top1-reverse"
#define FEEDER_WBR_INIT_ID 0x00090100UL
#define FEEDER_TUYA_MCU_VERSION "0.9.1"
#endif
#else
#define FEEDER_WBR_INIT_VERSION "0.9.0-top1-motor"
#define FEEDER_WBR_INIT_ID 0x00090000UL
#define FEEDER_TUYA_MCU_VERSION "0.9.0"
#endif
#elif defined(FEEDER_PUMP_TEST)
#define FEEDER_WBR_INIT_VERSION "0.8.0-pump-test"
#define FEEDER_WBR_INIT_ID 0x00080000UL
#define FEEDER_TUYA_MCU_VERSION "0.8.0"
#elif defined(FEEDER_LED_TEST)
#define FEEDER_WBR_INIT_VERSION "0.7.0-led-test"
#define FEEDER_WBR_INIT_ID 0x00070000UL
#define FEEDER_TUYA_MCU_VERSION "0.7.0"
#else
#define FEEDER_WBR_INIT_VERSION "0.6.8-wbr-pair"
#define FEEDER_WBR_INIT_ID 0x00060800UL
#define FEEDER_TUYA_MCU_VERSION "0.6.8"
#endif
/* 保留0.6.6的常配网m0，来源是本产品生成协议PDF第3页。
 * 用户已授权进入配网；0x04仅由显式Pair任务触发，没有自动重置或电机动作。
 */
#define FEEDER_TUYA_CONFIG_MODE "0"
#else
#define FEEDER_WBR_INIT_VERSION "0.5.0-wbr-init"
#define FEEDER_WBR_INIT_ID 0x00050000UL
#define FEEDER_TUYA_MCU_VERSION "0.5.0"
#define FEEDER_TUYA_CONFIG_MODE "1"
#endif
#define FEEDER_WBR_INIT_MAGIC 0x5742494EUL
/* 联网测试时填入自己的涂鸦产品 PID。公共台架固件使用占位值。 */
#ifndef FEEDER_TUYA_PID
#define FEEDER_TUYA_PID "replace_with_pid"
#endif
/* 配网模式按对应版本配置；重置仅由明确Pair任务触发。 */
#define FEEDER_TUYA_PRODUCT_JSON "{\"p\":\"" FEEDER_TUYA_PID "\",\"v\":\"" FEEDER_TUYA_MCU_VERSION "\",\"m\":" FEEDER_TUYA_CONFIG_MODE "}"
#define FEEDER_INIT_TX_TIMEOUT_MS 200U
#endif

#ifndef FEEDER_LED_TEST_CONFIG_H
#define FEEDER_LED_TEST_CONFIG_H
/* 唯一主图第3页：PB4(16脚) DIN -> Q2源极；栅极固定3.3V；
 * 漏极 -> TM1652 SDA(16脚)，两侧各有上拉。这是同相电平转换。
 * PB4采用开漏：0拉低，1释放，由外部上拉产生高电平。
 * U2的GR1~4接28颗数字段，GR5接LP1、LM1、L29、L30、L31。
 * V1.0图纸与V1.2实物的对应仍待点亮实测；SG8/GR6未接。
 */
#define FEEDER_LED_PIN 4U
#define FEEDER_LED_BIT_US 52U
#define FEEDER_LED_GAP_MS 6U
/* TM1652 V1.0第6页：7段6位、段电流2/8、位占空比4/16。
 * 1/8段电流可能不能点亮普通数码管，按手册从2/8开始。
 */
#define FEEDER_LED_CONTROL 0x94U
#endif

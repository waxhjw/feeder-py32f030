# 0.15.6 公共台架固件

| 文件 | 用途 |
|---|---|
| [完整烧录 HEX](feeder_0.15.6-public-bench.factory.hex) | 初次用 SWD 烧录的整片 64 KiB 镜像，包含 Boot、应用和活动描述。会重置 MCU 内存储的喂食计划，已有板先备份。 |
| [应用 BIN](feeder_0.15.6-public-bench.app.bin) | 主控应用二进制，链接地址 `0x08002000`，供分析或与对应 Boot 配合。 |
| [MCU OTA BIN](feeder_0.15.6-public-bench.ota.bin) | 带 128 字节描述头的升级包，供兼容 Boot 与涂鸦 MCU 通道测试。 |
| [SHA-256](SHA256SUMS.txt) | 下载后可核对三个文件的完整性。 |

上电台架验证时让 CN7 电机保持拔下，CN6 接微动。待机轻触 TOP2 选份，同时触摸 TOP1/TOP2 约 2 秒切换静音。按住 CN6 模拟停位，轻触 TOP1 启动，松开再按回模拟完成一份。待机单独长按 TOP2 约 5 秒会让联网模块进入配网流程。公开固件带占位 PID，自建产品的联网试验需先按仓库说明配置并重新构建。

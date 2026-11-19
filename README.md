# 自动喂食器 · PY32F030

基于 20 脚 PY32F030F18P6TU 和 WBR3 的喂食器开发项目。仓库提供主控源码、可下载固件、原理图和可编辑的 PCB 参考工程。

| 资料 | 文件 |
|---|---|
| 主控源码 | [应用](app)、[板级配置](board)、[驱动](bsp)、[入口与启动](src)、[测试源码](tests) |
| 固件 | [0.15.6 公共台架包及使用说明](firmware/README.md) |
| 原理图 | [Wi‑Fi 显示喂食器 PDF](hardware/WIFI-display-feeder-schematic.pdf)，主控图见第 3 页 |
| PCB | [嘉立创 EDA Pro 参考工程](hardware/ACKF-YK-W1-MAIN-VA-reference.eprj2)，[硬件版本差异](hardware/README.md) |

## 0.15.6 台架版

CN6 微动模拟计份，TOP1 启动或停止，TOP2 短按选份。数码管显示时间、份数和进度，无源蜂鸣器提供提示音。固件包含涂鸦 MCU 串口协议、计划保存、联网校时、重配网及 MCU OTA 逻辑。原项目实板已验证手机 OTA 和重配网保留计划。公开产物使用占位产品 PID，发布前已重新编译及检查镜像。在自己的涂鸦产品上测试联网时，需要填自己的 PID，并配置 WBR3 模组授权、App 面板及对应功能点。台架版锁停电机输出，手工模拟计份用于开发验证。

## Windows 构建

准备 Python 3、CMake、Arm GNU Toolchain 和 MinGW Make，加入 PATH。以下命令不连接主板。

```powershell
python tools/fetch-deps.py
$armBin = Split-Path -Parent (Get-Command arm-none-eabi-gcc.exe).Source
cmake -S . -B build -G "MinGW Makefiles" "-DCMAKE_TOOLCHAIN_FILE=cmake/arm-gcc.cmake" "-DARM_GCC_BIN=$armBin" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target feeder_ota_boot_1.0.0 feeder_cycle_net_diag_0.15.6 --parallel 4
python tools/package-public.py build firmware
```

联网开发时，可在 [板级联网配置](board/wbr_init_config.h) 填写自己的 `FEEDER_TUYA_PID` 后构建。快速喂食为 DP2，计划为 DP1（普通 Raw 整表），状态为 DP4，启停为 DP101/102，选份为 DP104。WBR3 模组固件及账号配置由各自开发者在涂鸦平台准备。

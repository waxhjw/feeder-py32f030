# 喂食器硬件资料

- [原理图 PDF](WIFI-display-feeder-schematic.pdf) 是本项目当前固件采用的 20 脚主控图。第 3 页图纸日期为 2026-07-29。CN6 接微动，CN7 为电机接口，PB5 驱动无源蜂鸣器。
- [可编辑 PCB 工程](ACKF-YK-W1-MAIN-VA-reference.eprj2) 用嘉立创 EDA 专业版打开，包含主板和第二小板轮廓。它是基于 `ACKF-YK-W1-main-V1.0(0902)` 的开发参考。已知参考工程的电池座是 CR2032，主控 PDF 标注 CR1220，R9 连接也有版本差异。制板前按目标硬件版本复核原理图、结构和设计规则，并从 EDA 导出 Gerber 和钻孔文件。

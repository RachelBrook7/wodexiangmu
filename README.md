# 交通灯控制系统（AT89C51, 12MHz, 共阴数码管）

本项目包含：
- 基础功能（80分）：红/黄/绿交通灯循环显示；步进电机模拟道闸的升降（ULN2003 驱动 28BYJ‑48）
- 扩展功能（20分）：4×4 矩阵键盘设定红/黄/绿的时间周期；四位共阴数码管动态扫描倒计时显示

器件与约定：
- 单片机：AT89C51
- 晶振：12 MHz
- 数码管：四位共阴（Common Cathode），位选低电平有效
- 键盘：4×4 矩阵（行 P3.0–P3.3，列 P3.4–P3.7）
- 步进电机：28BYJ‑48 + ULN2003A（P1.0–P1.3）

目录结构（将逐步补齐）：
- proteus/（稍后上传：.pdsprj/.dsn 原理图与连线）
- keil/
  - traffic_system_51.c（源码）
  - （稍后上传：Keil 工程 .uvproj/.uvopt）
  - build/traffic_system_51.hex（稍后上传：已编译的 HEX）
- docs/
  - WIRING.md（连线图）
  - PROTEUS_STEPS.md（Proteus 搭建步骤）

快速开始：
1. 使用 Keil C51 打开/新建工程，加入 `keil/traffic_system_51.c`，目标 AT89C51，晶振 12MHz，编译得到 HEX。
2. 打开 Proteus 8，按 `docs/WIRING.md` 连线；在 MCU 属性里加载 HEX；按 `docs/PROTEUS_STEPS.md` 运行仿真。

完成首次提交后，回复“ready”，我会把完整的 Proteus 项目（.pdsprj/.dsn）与 Keil 工程（.uvproj/.uvopt）推送到该仓库。

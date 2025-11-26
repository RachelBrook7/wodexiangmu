# Proteus 8 搭建与仿真步骤

1) 创建工程并添加器件：
   - AT89C51、Crystal(12MHz)、Capacitor(22pF×2, 0.1µF)、Resistor
   - ULN2003A、28BYJ-48（或 Unipolar Stepper）、Matrix Keypad
   - 7-Segment 4-Digit（Common Cathode）、LED×3

2) 按 WIRING.md 连线，设置属性：
   - MCU Clock: 12.000000 MHz
   - 数码管类型：Common Cathode（共阴）；位选低电平有效
   - ULN2003A COM 接 +5V；电机供电 5V；所有地相连

3) 加载程序：
   - 使用 Keil C51 编译 `keil/traffic_system_51.c` 生成 HEX
   - 在 Proteus 中双击 MCU，Program File 指向该 HEX

4) 运行：
   - 上电红灯亮、倒计时显示
   - 倒计时到 0 → 绿灯并抬杆（步进电机正转）
   - 绿→黄→红循环，红灯时落杆（反转）
   - 键盘：A/B/C 分别进入红/黄/绿设置；输入 0–9（最多4位）后按 D 提交；* 清除，# 恢复默认（10/3/10）

# 竞品与替代方案分析

> 目标场景：让嵌入式 GUI（LVGL 等）的**模拟器**可以被**程序化操作**，并把过程
> **录制成视频**（或截图/断言），用于演示、文档、CI 回归测试。
> 本文档对比现有方案，并分析每条路线的取舍。

## 一图流：方案定位

```
                 轻量 ←———————————————→ 重量
  本项目(桥接) → WASM(编译进网页) → VNC/串流 → 全板级仿真(Renode)
  工具链成本:   零                emsdk        VNC栈        Renode+平台包
  自动化:       Playwright        Playwright   noVNC页面     Robot框架
  录视频:       Playwright        Playwright   另想办法      dump拼帧
```

## 竞品详评

### 1. LVGL 官方在线 Demo（WASM）

- **是什么**：[lvgl.io/demos](https://lvgl.io/demos) 上直接跑的官方控件演示，
  Emscripten 编译，鼠标可操作
- **亮点**：零安装、官方维护、展示 LVGL 能力的门面
- **局限**：只能看官方 demo，**无法加载你自己的 UI 工程**
- **与本项目差异**：官方 demo 是"展示柜"，本项目是"你自己的工程进浏览器"

### 2. lv_sim_emscripten（官方 WASM 模板）

- **是什么**：[lvgl/lv_sim_emscripten](https://github.com/lvgl/lv_sim_emscripten)，
  把 LVGL + 你的 UI 编译成 WASM 在浏览器跑的官方模板工程
- **亮点**：正统路线，产物纯前端静态文件，可部署到任何静态托管
- **局限**：
  - 需要 emsdk 工具链（国内网络极难安装，`storage.googleapis.com` 被墙）
  - 每次改 UI 都要重交叉编译 wasm，迭代慢
  - 调试走 wasm 符号，比原生 gdb 体验差
  - 自动化要自己在页面 JS 上加钩子，没有现成录制方案
- **与本项目差异**：WASM 把 UI **编译**进浏览器，本项目把 UI **串流**进浏览器；
  前者部署形态更纯（纯静态），后者迭代和自动化成本更低

### 3. MicroPython WASM 模拟器

- **是什么**：MicroPython + LVGL 编译成 WASM，在浏览器里写 Python 跑 UI
  （[lvgl#1320](https://github.com/lvgl/lvgl/issues/1320)）
- **亮点**：交互式学习 LVGL API 的好方式
- **局限**：面向 MicroPython 生态；C 工程不适用；同样受 emsdk 制约
- **与本项目差异**：受众不同（学 API vs 工程自动化）

### 4. Wokwi（云端单片机仿真）

- **是什么**：[wokwi.com](https://wokwi.com)，浏览器里仿真 ESP32/Arduino 等，
  支持 SSD1306/ILI9341 等显示器件，能跑 LVGL
- **亮点**：完整 MCU 外设级仿真，零本地环境，教学体验极佳
- **局限**：免费档受限（项目数/外设数），编译走云端；UI 自动化 API 有限，
  没有视频录制；大工程/私有代码不便上云
- **与本项目差异**：Wokwi 仿"硬件"，本项目跑"你已编译好的原生 UI 代码"；
  私有工程/自动化场景本项目完胜，教学场景 Wokwi 更好

### 5. InfiniSim（手表固件模拟器标杆）

- **是什么**：[InfiniTimeOrg/InfiniSim](https://github.com/InfiniTimeOrg/InfiniSim)，
  InfiniTime 智能手表的官方 PC 模拟器，SDL 桌面窗口，GitHub Actions 出 CI 构建
- **亮点**：与本项目的"出身"最像——手表固件 + LVGL + PC 模拟器 + CI；
  证明"模拟器即开发入口"路线在手表社区是成立的
- **局限**：桌面 SDL 窗口，自动化/录制需要自己抓屏（Linux Xvfb+ffmpeg 可做，
  Windows 难）；无浏览器形态
- **与本项目差异**：InfiniSim 止步于"人工看"，本项目补上了"程序操作+录制"那一环

### 6. Renode（全板级仿真框架）

- **是什么**：[renode.io](https://renode.io)，Antmicro 开源的嵌入式整板仿真
  （CPU/外设/传感器），可跑真固件 ELF，支持屏幕外设，带 Robot Framework 测试集成
- **亮点**：工业级，能仿真到"整机行为"，测试能力最严肃
- **局限**：学习曲线陡；需要为目标板写平台描述；渲染性能一般；
  录视频要自己拼屏幕 dump
- **与本项目差异**：Renode 解决"整机验证"，本项目解决"UI 展示与自动化"，
  量级差一个数量级，不构成直接竞争

### 7. 屏幕录制 + 全局键鼠脚本（OBS / ffmpeg / AutoHotkey）

- **是什么**：最朴素路线：脚本驱动真实鼠标键盘（SendInput 级），
  OBS/ffmpeg gdigrab 抓窗口
- **局限**：SDL 窗口在 UIA 树里只有一个空壳，无法按控件定位，只能盲坐标；
  会抢真实键鼠焦点（录制期间不能用电脑）；受 DPI/窗口位置/前台切换影响；
  无断言能力
- **与本项目差异**：本项目事件注入到 LVGL 输入设备层（比操作系统层更稳），
  Playwright 录制不占真实键鼠

## 替代方案矩阵（为达成"自动操作 + 录视频"）

| 方案 | 工具链成本 | 自动化稳定性 | 录视频 | CI 友好 | 备注 |
|---|---|---|---|---|---|
| **本项目（桥接+Playwright）** | 低（本机 gcc + Node） | 高（注入 LVGL 输入层） | ✅ 内置 | ✅ headless | 需给模拟器加 net_hal |
| WASM + Playwright | 高（emsdk） | 高 | ✅ 内置 | ✅ | 国内网络 emsdk 难装；部署形态最纯 |
| VNC/noVNC + Playwright | 中（VNC server/Xvfb） | 中（坐标换算+延迟） | 需另录 | 中 | 通用性强，层次多 |
| 原生键鼠 + OBS/AHK | 低 | 低（抢焦点/DPI） | ✅ | ✗ | 人工演示够用 |
| lv_utest 金图对比 | 低 | 高（LVGL 官方测试体系） | ✗（仅 pass/fail） | ✅ | 与本项目互补 |
| Renode + Robot | 高 | 高 | 拼帧 | ✅ | 面向整机验证 |
| 商业 GUI 测试（Squish 等） | 高（授权费） | 中（canvas 无控件树） | ✅ | ✅ | 性价比低 |

## 结论

1. "LVGL 进浏览器"已有成熟路线（WASM），但都停在**人看**；
   "**程序操作 + 录制**"这一环是空白
2. 录视频需求下，Playwright 是同时满足"稳定注入 + 免键鼠抢占 + 内置录制 +
   无头 CI"的最短路径
3. 若未来需要纯静态部署（如在线 Demo 页），WASM 是互补而非替代；
   两者可以共享同一套 Playwright 脚本

# LVGL Web Bridge — 通信协议

桥接层只有两种消息，都是**小端字节序**、极简二进制格式，任何语言 10 行代码即可对接。

## 1. 帧消息（C 端 → 浏览器，exe stdout）

每帧一个消息，**只在 LVGL 产生重绘时发送**（挂在 `monitor_cb` 钩子上），
空闲时零流量：

```
偏移  长度  类型      含义
0     4    u32 LE   魔数 0x4C565331 ("LVS1")
4     4    u32 LE   屏幕宽 w (像素)
8     4    u32 LE   屏幕高 h (像素)
12    w*h*4  bytes  RGBA8888 像素, 行主序, 从左上角开始
```

帧长固定 `12 + w*h*4`。服务端解析时若魔数不匹配会**逐字节滑动重同步**，
因此流上混入任何脏数据都能自动恢复。

## 2. 指针事件（浏览器 → C 端，exe stdin）

固定 5 字节：

```
偏移  长度  类型      含义
0     1    u8        0 = 移动   1 = 按下   2 = 松开
1     2    u16 LE    x (屏幕像素坐标)
3     2    u16 LE    y (屏幕像素坐标)
```

C 端后台线程阻塞读取，更新共享状态（临界区保护）；
LVGL 的 pointer indev 每 30ms 轮询一次该状态。

## 3. C 端与 stdout 日志的隔离

LVGL 的 `LV_LOG_PRINTF` 模式会把日志直接 `printf` 到 stdout，会污染帧流。
`net_hal.c` 的做法：

```c
int frame_fd = _dup(_fileno(stdout));       /* 复制原管道给帧流 */
_dup2(_fileno(stderr), _fileno(stdout));    /* stdout 本体改道到 stderr */
frame_out = _fdopen(frame_fd, "wb");
```

日志照常可见（走 stderr），帧流保持纯净。

## 4. 已知约定

- 当前像素格式为 RGB565→RGBA 转换（对应 `LV_COLOR_DEPTH 16`），
  32 位色工程改 `display_flush_cb` 里的转换即可
- 多个浏览器客户端同时连接时，服务端**广播同一份输入流**
  （所有人共享一块屏，适合结对/演示场景）
- 帧率 = LVGL 刷新率（`LV_DISP_DEF_REFR_PERIOD`，默认 33ms），
  页面不做插帧

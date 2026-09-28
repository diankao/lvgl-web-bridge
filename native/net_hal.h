#ifndef NET_HAL_H
#define NET_HAL_H

/* 无头网络模式 HAL：替代 SDL 后端
 * - 显示: 每帧完成后把整屏 RGBA 帧写入 stdout  [magic u32][w u32][h u32][RGBA...]
 * - 输入: 后台线程从 stdin 读 5 字节鼠标事件   [ev u8][x u16le][y u16le]
 *   ev: 0=移动 1=按下 2=松开
 */
void net_hal_init(int w, int h);
void net_hal_silence_logs(void); /* 必须在 lv_init() 之前调用 */

#endif

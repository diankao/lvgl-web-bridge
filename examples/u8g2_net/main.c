/* net_hello — u8g2 无头网络模式示例 (LVGL Web Bridge 同款 LVS1 协议)
 *
 * 显示: 每帧把 u8x8_bitmap 位图转 RGBA, 按协议写 stdout
 *       [magic u32][w u32][h u32][RGBA...]   (无重绘抑制, 定时全帧推送)
 * 输入: 暂未桥接 (动画示例不需要; 交互示例可按需把 stdin 事件映射到按键)
 *
 * 构建后配合 web/server.js:  SIM_EXE=<本目录>/u8g2_net.exe node server.js
 */
#include "u8g2.h"
#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

/* u8x8_d_bitmap.c 导出但头文件未声明 */
extern uint8_t u8x8_GetBitmapPixel(u8x8_t *u8x8, uint16_t x, uint16_t y);

u8g2_t u8g2;

static void net_send_frame(void)
{
    u8x8_t *u8x8 = u8g2_GetU8x8(&u8g2);
    uint32_t w = (uint32_t)u8x8_GetCols(u8x8) * 8;
    uint32_t h = (uint32_t)u8x8_GetRows(u8x8) * 8;

    uint8_t hdr[12];
    uint32_t magic = 0x4C565331; /* "LVS1" */
    memcpy(hdr, &magic, 4);
    memcpy(hdr + 4, &w, 4);
    memcpy(hdr + 8, &h, 4);
    fwrite(hdr, 1, 12, stdout);

    /* 1bpp -> RGBA, 点亮=白(OLED 风格) */
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t on = u8x8_GetBitmapPixel(u8x8, (uint16_t)x, (uint16_t)y);
            uint8_t px[4] = {on ? 255 : 0, on ? 255 : 0, on ? 255 : 0, 255};
            fwrite(px, 1, 4, stdout);
        }
    }
    fflush(stdout);
}

int main(void)
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 128x64 全缓冲 bitmap 设备 —— 不需要 SDL */
    u8g2_SetupBitmap(&u8g2, U8G2_R0, 128, 64);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    u8g2_SetFont(&u8g2, u8g2_font_7x14B_tr);
    u8g2_SetFontDirection(&u8g2, 0);

    int x = 10, dx = 2;
    int frame = 0;

    for (;;) {
        u8g2_ClearBuffer(&u8g2);

        u8g2_DrawStr(&u8g2, x, 20, "u8g2 -> Web!");
        u8g2_DrawFrame(&u8g2, x - 2, 10, 90, 14);
        u8g2_DrawStr(&u8g2, 4, 45, "LVGL Web Bridge");
        u8g2_DrawHLine(&u8g2, 4, 50, 120);

        char buf[16];
        sprintf(buf, "frame %d", frame);
        u8g2_DrawStr(&u8g2, 4, 62, buf);

        u8g2_SendBuffer(&u8g2);
        net_send_frame();

        x += dx;
        if (x <= 2 || x >= 40) dx = -dx;
        frame++;
        SLEEP_MS(50); /* 20 fps */
    }
    return 0;
}

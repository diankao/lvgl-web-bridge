/* net_hal.c — 无头网络模式 HAL（Playwright/浏览器 远程驱动用）
 * 显示: LVGL flush_cb 把脏区转成 RGBA 写进整屏缓冲, monitor_cb 在帧完成时写 stdout
 * 输入: stdin 读线程收鼠标事件, 供 LVGL pointer indev 轮询
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "net_hal.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif

static int SCR_W, SCR_H;
static uint8_t *frame_rgba;          /* 整屏 RGBA 缓冲 */
static lv_color_t *draw_buf;         /* LVGL 绘制缓冲 */
static FILE *frame_out;              /* 帧输出句柄: 原 stdout 管道(stdout 本体已改道给日志) */

/* ---- 鼠标状态（读线程写，LVGL 轮询读） ---- */
#ifdef _WIN32
static CRITICAL_SECTION mouse_cs;
#endif
static volatile int mouse_x, mouse_y;
static volatile int mouse_pressed;

static void display_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    int x1 = area->x1 < 0 ? 0 : area->x1;
    int y1 = area->y1 < 0 ? 0 : area->y1;
    int x2 = area->x2 >= SCR_W ? SCR_W - 1 : area->x2;
    int y2 = area->y2 >= SCR_H ? SCR_H - 1 : area->y2;

    for (int y = y1; y <= y2; y++) {
        const lv_color_t *src = color_p + (y - area->y1) * (area->x2 - area->x1 + 1) + (x1 - area->x1);
        uint8_t *dst = frame_rgba + (y * SCR_W + x1) * 4;
        for (int x = x1; x <= x2; x++) {
            uint16_t c = src->full;                      /* RGB565 */
            dst[0] = (uint8_t)(((c >> 11) & 0x1F) << 3); /* R */
            dst[1] = (uint8_t)(((c >> 5)  & 0x3F) << 2); /* G */
            dst[2] = (uint8_t)(((c)       & 0x1F) << 3); /* B */
            dst[3] = 0xFF;                               /* A */
            src++; dst += 4;
        }
    }
    lv_disp_flush_ready(drv);
}

static void frame_send_cb(lv_disp_drv_t *drv, uint32_t time, uint32_t px)
{
    (void)drv; (void)time; (void)px;
    uint8_t hdr[12];
    uint32_t magic = 0x4C565331; /* "LVS1" */
    memcpy(hdr, &magic, 4);
    memcpy(hdr + 4, &SCR_W, 4);
    memcpy(hdr + 8, &SCR_H, 4);
    fwrite(hdr, 1, 12, frame_out);
    fwrite(frame_rgba, 1, (size_t)SCR_W * SCR_H * 4, frame_out);
    fflush(frame_out);
}

static void mouse_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
#ifdef _WIN32
    EnterCriticalSection(&mouse_cs);
#endif
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
#ifdef _WIN32
    LeaveCriticalSection(&mouse_cs);
#endif
}

#ifdef _WIN32
static DWORD WINAPI tick_thread(LPVOID param)
{
    /* net 模式没有 SDL 驱动的 tick 线程, 这里自己推进 LVGL 时钟 */
    (void)param;
    for (;;) {
        lv_tick_inc(5);
        Sleep(5);
    }
    return 0;
}

static DWORD WINAPI stdin_reader(LPVOID param)
{
    (void)param;
    for (;;) {
        uint8_t ev[5];
        size_t n = fread(ev, 1, 5, stdin);
        if (n != 5) break; /* stdin 关闭，退出线程 */
        uint16_t x = (uint16_t)(ev[1] | (ev[2] << 8));
        uint16_t y = (uint16_t)(ev[3] | (ev[4] << 8));
        EnterCriticalSection(&mouse_cs);
        mouse_x = x; mouse_y = y;
        if (ev[0] == 1) mouse_pressed = 1;
        else if (ev[0] == 2) mouse_pressed = 0;
        LeaveCriticalSection(&mouse_cs);
    }
    return 0;
}
#endif

static void noop_log_cb(const char *buf)
{
    (void)buf; /* 网络模式下 stdout 传输二进制帧，日志必须静默 */
}

void net_hal_silence_logs(void)
{
    lv_log_register_print_cb(noop_log_cb);
}

void net_hal_init(int w, int h)
{
    SCR_W = w; SCR_H = h;
    frame_rgba = (uint8_t *)malloc((size_t)w * h * 4);
    draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * (size_t)w * h);

#ifdef _WIN32
    {
        /* 保住原 stdout 管道给帧流用；stdout 本体改道到 stderr，
         * LVGL 的 printf 日志继续可见但不再混进二进制帧 */
        int frame_fd = _dup(_fileno(stdout));
        _setmode(frame_fd, _O_BINARY);
        _dup2(_fileno(stderr), _fileno(stdout));
        frame_out = _fdopen(frame_fd, "wb");
        setvbuf(frame_out, NULL, _IONBF, 0);
        _setmode(_fileno(stdin), _O_BINARY);
    }
    InitializeCriticalSection(&mouse_cs);
    CreateThread(NULL, 0, tick_thread, NULL, 0, NULL);
    CreateThread(NULL, 0, stdin_reader, NULL, 0, NULL);
#endif

    static lv_disp_draw_buf_t db;
    lv_disp_draw_buf_init(&db, draw_buf, NULL, w * h);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &db;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.monitor_cb = frame_send_cb;
    disp_drv.hor_res = w;
    disp_drv.ver_res = h;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    lv_theme_t *th = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                           lv_palette_main(LV_PALETTE_RED),
                                           LV_THEME_DEFAULT_DARK, LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, th);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read_cb;
    lv_indev_t *indev = lv_indev_drv_register(&indev_drv);
    (void)indev;
}

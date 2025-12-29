/**
 * @file display_fbdev.c
 * @brief F133 Linux 平台显示驱动实现（framebuffer）
 * 
 * 关键策略：
 * - 使用 FBdev + partial refresh
 * - 禁用全屏刷新（full_refresh = 0）
 * - 分区域刷新降低功耗
 * 
 * 注意：此文件为框架模板，需要根据实际 F133 硬件配置调整
 */

#include "drivers/display_driver.h"
#include <lvgl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

// F133 framebuffer 设备路径（根据实际调整）
#define FB_DEVICE "/dev/fb0"

static int fb_fd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static uint32_t *fb_mem = NULL;
static size_t fb_size = 0;

static int display_fbdev_init(void) {
    fprintf(stderr, "[FBDEV] Initializing framebuffer...\n");
    
    fb_fd = open(FB_DEVICE, O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "[FBDEV] Failed to open %s\n", FB_DEVICE);
        return 0;
    }
    
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "[FBDEV] Failed to get variable screen info\n");
        close(fb_fd);
        return 0;
    }
    
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        fprintf(stderr, "[FBDEV] Failed to get fixed screen info\n");
        close(fb_fd);
        return 0;
    }
    
    fb_size = finfo.smem_len;
    fb_mem = (uint32_t*)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        fprintf(stderr, "[FBDEV] Failed to mmap framebuffer\n");
        close(fb_fd);
        return 0;
    }
    
    fprintf(stderr, "[FBDEV] Framebuffer initialized: %dx%d, %d bpp\n", 
            vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    return 1;
}

static void display_fbdev_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    if (fb_mem == NULL || fb_fd < 0) {
        lv_disp_flush_ready(drv);
        return;
    }
    
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;
    
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= (int32_t)vinfo.xres) x2 = vinfo.xres - 1;
    if (y2 >= (int32_t)vinfo.yres) y2 = vinfo.yres - 1;
    
    if (x1 > x2 || y1 > y2) {
        lv_disp_flush_ready(drv);
        return;
    }
    
    int32_t w = (x2 - x1 + 1);
    int32_t h = (y2 - y1 + 1);
    
    // 颜色格式转换：LVGL -> FBdev
    // 注意：需要根据实际 F133 framebuffer 格式调整（可能是 RGB565, ARGB8888 等）
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            size_t src_idx = y * w + x;
            size_t dst_x = x1 + x;
            size_t dst_y = y1 + y;
            size_t dst_idx = dst_y * vinfo.xres + dst_x;
            
            if (dst_idx >= fb_size / sizeof(uint32_t)) break;
            
            lv_color_t color = color_p[src_idx];
            // 转换为 ARGB8888（根据实际格式调整）
            uint32_t argb = ((uint32_t)color.ch.alpha << 24) | 
                           ((uint32_t)color.ch.red << 16) | 
                           ((uint32_t)color.ch.green << 8) | 
                           (uint32_t)color.ch.blue;
            fb_mem[dst_idx] = argb;
        }
    }
    
    // 注意：F133 上可能不需要手动刷新，framebuffer 是直接映射的
    // 如果需要，可以调用 ioctl(fb_fd, FBIO_WAITFORVSYNC, ...)
    
    lv_disp_flush_ready(drv);
}

static void display_fbdev_deinit(void) {
    if (fb_mem && fb_mem != MAP_FAILED) {
        munmap(fb_mem, fb_size);
        fb_mem = NULL;
    }
    if (fb_fd >= 0) {
        close(fb_fd);
        fb_fd = -1;
    }
    fprintf(stderr, "[FBDEV] Display deinitialized\n");
}

static bool display_fbdev_get_resolution(int32_t *width, int32_t *height) {
    if (fb_fd < 0) return false;
    if (width) *width = vinfo.xres;
    if (height) *height = vinfo.yres;
    return true;
}

// 导出接口实例
display_iface_t DISPLAY = {
    .init = display_fbdev_init,
    .flush = display_fbdev_flush,
    .deinit = display_fbdev_deinit,
    .get_resolution = display_fbdev_get_resolution
};


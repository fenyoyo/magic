#ifndef _OLED_DISPLAY_H_
#define _OLED_DISPLAY_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 使用与 MPU6050 相同的 I2C 总线初始化 OLED (SSD1306 128x64)，成功返回 true */
bool oled_init(void);

/** 清屏 */
void oled_clear(void);

/** 刷新显存到屏幕（在多次 oled_draw_text 后调用一次即可） */
void oled_refresh(void);

/**
 * 在指定行 (0~7，每行约 8 像素高) 绘制字符串，支持 0-9 . - 空格及部分字母。
 * 内部会刷新，适合逐行更新。
 */
void oled_draw_line(int line, const char *str);

/**
 * 显示陀螺仪和加速度：gx,gy,gz 为角速度 °/s，ax,ay,az 为加速度 g。
 * 会清屏并绘制 6 行文本后刷新。
 */
void oled_show_imu(float gx, float gy, float gz, float ax, float ay, float az);

#ifdef __cplusplus
}
#endif

#endif

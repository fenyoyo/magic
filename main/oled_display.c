/**
 * SSD1306 128x64 I2C OLED 简易驱动，与 MPU6050 共用 I2C 总线。
 * 使用 i2cdev，地址 0x3C。
 */
#include "oled_display.h"
#include "i2cdev.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdio.h>

#define TAG           "OLED"
#define SSD1306_ADDR  0x3C
#define SSD1306_W     128
#define SSD1306_H     64
#define SSD1306_PAGES (64 / 8)

#ifdef CONFIG_EXAMPLE_SDA_GPIO
#define OLED_SDA_GPIO (gpio_num_t)CONFIG_EXAMPLE_SDA_GPIO
#define OLED_SCL_GPIO (gpio_num_t)CONFIG_EXAMPLE_SCL_GPIO
#else
#define OLED_SDA_GPIO GPIO_NUM_8
#define OLED_SCL_GPIO GPIO_NUM_9
#endif

#define I2C_PORT      I2C_NUM_0

static i2c_dev_t s_dev;
static bool s_inited;
static uint8_t s_fb[SSD1306_W * SSD1306_PAGES];

static esp_err_t oled_cmd(uint8_t cmd)
{
    uint8_t buf[] = { 0x00, cmd };
    return i2c_dev_write(&s_dev, buf, sizeof(buf), NULL, 0);
}

static esp_err_t oled_cmds(const uint8_t *cmds, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        esp_err_t e = oled_cmd(cmds[i]);
        if (e != ESP_OK) return e;
    }
    return ESP_OK;
}

static esp_err_t oled_data(const uint8_t *data, size_t len)
{
    if (len == 0) return ESP_OK;
    uint8_t buf[33];
    buf[0] = 0x40;
    size_t off = 0;
    while (off < len)
    {
        size_t n = len - off;
        if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
        memcpy(buf + 1, data + off, n);
        esp_err_t err = i2c_dev_write(&s_dev, buf, 1 + n, NULL, 0);
        if (err != ESP_OK) return err;
        off += n;
    }
    return ESP_OK;
}

/* 6x8 字体：每字符 6 列，每列 1 字节 (LSB 在上)，索引为 ASCII-32 */
static const uint8_t s_font_6x8[96][6] = {
    [0]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* space */
    [13] = { 0x00, 0x00, 0x1C, 0x1C, 0x00, 0x00 },  /* - */
    [14] = { 0x00, 0x60, 0x60, 0x00, 0x00, 0x00 },  /* . */
    [16] = { 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 },  /* 0 */
    [17] = { 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 },  /* 1 */
    [18] = { 0x62, 0x51, 0x49, 0x49, 0x46, 0x00 },  /* 2 */
    [19] = { 0x22, 0x49, 0x49, 0x49, 0x36, 0x00 },  /* 3 */
    [20] = { 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 },  /* 4 */
    [21] = { 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 },  /* 5 */
    [22] = { 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00 },  /* 6 */
    [23] = { 0x01, 0x71, 0x09, 0x05, 0x03, 0x00 },  /* 7 */
    [24] = { 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 },  /* 8 */
    [25] = { 0x06, 0x49, 0x49, 0x29, 0x1E, 0x00 },  /* 9 */
    [26] = { 0x00, 0x36, 0x36, 0x00, 0x00, 0x00 },  /* : */
    [9]  = { 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00 },  /* A */
    [15] = { 0x3E, 0x41, 0x49, 0x49, 0x7A, 0x00 }, /* G */
    [56] = { 0x63, 0x14, 0x08, 0x14, 0x63, 0x00 }, /* X */
    [57] = { 0x07, 0x08, 0x70, 0x08, 0x07, 0x00 }, /* Y */
    [58] = { 0x61, 0x51, 0x49, 0x45, 0x43, 0x00 }, /* Z */
    [65] = { 0x20, 0x54, 0x54, 0x54, 0x78, 0x00 }, /* a */
    [67] = { 0x38, 0x44, 0x44, 0x44, 0x20, 0x00 }, /* c */
    [71] = { 0x3C, 0x4A, 0x4A, 0x4A, 0x34, 0x00 }, /* g */
    [82] = { 0x00, 0x7C, 0x08, 0x04, 0x00, 0x00 }, /* r */
    [88] = { 0x44, 0x28, 0x10, 0x28, 0x44, 0x00 }, /* x */
    [89] = { 0x04, 0x08, 0x70, 0x08, 0x04, 0x00 }, /* y */
    [90] = { 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00 }, /* z */
    [5]  = { 0x7C, 0x0A, 0x11, 0x22, 0x7C, 0x00 }, /* % */
    [70] = { 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00 }, /* f */
};

static inline unsigned font_idx(char c)
{
    unsigned i = (unsigned)(uint8_t)c - 32u;
    return i < 96u ? i : 0u;
}

static void oled_draw_char(int x, int page, char c)
{
    if (x + 6 > SSD1306_W || page < 0 || page >= SSD1306_PAGES) return;
    const uint8_t *glyph = s_font_6x8[font_idx(c)];
    for (int col = 0; col < 6; col++)
        s_fb[page * SSD1306_W + x + col] = glyph[col];
}

static void oled_draw_str(int x, int page, const char *str)
{
    while (*str && x + 6 <= SSD1306_W)
    {
        oled_draw_char(x, page, *str);
        x += 6;
        str++;
    }
}

void oled_draw_line(int line, const char *str)
{
    if (!s_inited || line < 0 || line >= SSD1306_PAGES) return;
    memset(&s_fb[line * SSD1306_W], 0, SSD1306_W);
    int x = 0;
    while (*str && x + 6 <= SSD1306_W)
    {
        oled_draw_char(x, line, *str);
        x += 6;
        str++;
    }
    oled_cmd(0x21); oled_cmd(0); oled_cmd(SSD1306_W - 1);
    oled_cmd(0x22); oled_cmd(line); oled_cmd(line);
    oled_data(&s_fb[line * SSD1306_W], SSD1306_W);
}

void oled_clear(void)
{
    if (!s_inited) return;
    memset(s_fb, 0, sizeof(s_fb));
    oled_cmd(0x21); oled_cmd(0); oled_cmd(SSD1306_W - 1);
    oled_cmd(0x22); oled_cmd(0); oled_cmd(SSD1306_PAGES - 1);
    uint8_t zero[32];
    memset(zero, 0, sizeof(zero));
    for (int i = 0; i < (int)(sizeof(s_fb) / sizeof(zero)); i++)
        oled_data(zero, sizeof(zero));
}

void oled_refresh(void)
{
    if (!s_inited) return;
    oled_cmd(0x21); oled_cmd(0); oled_cmd(SSD1306_W - 1);
    oled_cmd(0x22); oled_cmd(0); oled_cmd(SSD1306_PAGES - 1);
    oled_data(s_fb, sizeof(s_fb));
}

bool oled_init(void)
{
    if (s_inited) return true;

    memset(&s_dev, 0, sizeof(s_dev));
    s_dev.port = I2C_PORT;
    s_dev.addr = SSD1306_ADDR;
    s_dev.cfg.sda_io_num = OLED_SDA_GPIO;
    s_dev.cfg.scl_io_num = OLED_SCL_GPIO;
#if defined(HELPER_TARGET_IS_ESP32) || defined(CONFIG_IDF_TARGET_ESP32)
    s_dev.cfg.master.clk_speed = 400000;
#endif

    if (i2c_dev_create_mutex(&s_dev) != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_dev_create_mutex failed");
        return false;
    }

    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    if (oled_cmds(init, sizeof(init)) != ESP_OK)
    {
        ESP_LOGE(TAG, "SSD1306 init cmds failed");
        i2c_dev_delete_mutex(&s_dev);
        return false;
    }

    memset(s_fb, 0, sizeof(s_fb));
    s_inited = true;
    ESP_LOGI(TAG, "OLED init OK (0x%02X)", SSD1306_ADDR);
    return true;
}

void oled_show_imu(float gx, float gy, float gz, float ax, float ay, float az)
{
    if (!s_inited) return;

    memset(s_fb, 0, sizeof(s_fb));
    char buf[22];

    snprintf(buf, sizeof(buf), "Gx:%6.2f Gy:%6.2f", gx, gy);
    oled_draw_str(0, 0, buf);
    snprintf(buf, sizeof(buf), "gz:%6.2f", gz);
    oled_draw_str(0, 1, buf);
    snprintf(buf, sizeof(buf), "Ax:%5.2f Ay:%5.2f", ax, ay);
    oled_draw_str(0, 2, buf);
    snprintf(buf, sizeof(buf), "Az:%5.2f", az);
    oled_draw_str(0, 3, buf);

    oled_refresh();
}

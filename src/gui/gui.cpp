#include <Arduino.h>
#include <Wire.h>
#include <lv_conf.h>
#include <lvgl.h>
#include "gui.h"
#include "../ui/ui.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "../rgb/rgb_gpio.h"

static const char *TAG = "gui";

static const uint16_t screenWidth = 800;
static const uint16_t screenHeight = 480;
static const uint16_t kStripeLines = 20;
static const uint32_t kPclkHz = 12000000;

static const int kPinData[16] = {
    15, 7, 6, 5, 4, 9, 46, 3, 8, 16, 1, 14, 21, 47, 48, 45,
};

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *s_stripe = nullptr;
static esp_lcd_panel_handle_t s_panel = nullptr;
static uint8_t s_touch_addr = 0;

static bool gt911_write(uint8_t reg_hi, uint8_t reg_lo, uint8_t value)
{
    Wire.beginTransmission(s_touch_addr);
    Wire.write(reg_hi);
    Wire.write(reg_lo);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool gt911_read(uint16_t *x, uint16_t *y)
{
    if (s_touch_addr == 0) {
        return false;
    }
    Wire.beginTransmission(s_touch_addr);
    Wire.write(0x81);
    Wire.write(0x4E);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom((int)s_touch_addr, 8) < 8) {
        return false;
    }
    uint8_t status = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t xh = Wire.read();
    uint8_t yl = Wire.read();
    uint8_t yh = Wire.read();
    (void)Wire.read();
    (void)Wire.read();
    (void)Wire.read();
    if ((status & 0x80) == 0) {
        return false;
    }
    gt911_write(0x81, 0x4E, 0);
    if ((status & 0x0F) == 0) {
        return false;
    }
    *x = (uint16_t)xl | ((uint16_t)xh << 8);
    *y = (uint16_t)yl | ((uint16_t)yh << 8);
    return true;
}

static bool gt911_probe(uint8_t addr)
{
    Wire.beginTransmission(addr);
    Wire.write(0x81);
    Wire.write(0x4E);
    return Wire.endTransmission() == 0;
}

static void touch_init()
{
    pinMode(38, OUTPUT);
    digitalWrite(38, LOW);
    delay(20);
    digitalWrite(38, HIGH);
    delay(100);

    Wire.begin(19, 20, 400000);
    if (gt911_probe(0x14)) {
        s_touch_addr = 0x14;
    } else if (gt911_probe(0x5D)) {
        s_touch_addr = 0x5D;
    }
    Serial.printf("[gui] GT911 addr=0x%02X\n", s_touch_addr);
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    const int x1 = area->x1;
    const int y1 = area->y1;
    const int x2 = area->x2 + 1;
    const int y2 = area->y2 + 1;

    if (s_panel != nullptr) {
        esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2, y2, &color_p->full);
    }

    if (g_phy_pending) {
        g_phy_pending = false;
        rgb_gpio_set_active_low(g_phy_r, g_phy_g, g_phy_b);
    }

    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    (void)indev_driver;
    uint16_t touchX = 0;
    uint16_t touchY = 0;
    data->state = LV_INDEV_STATE_REL;
    if (gt911_read(&touchX, &touchY)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

static bool rgb_panel_init()
{
    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_PLL160M;
    cfg.timings.pclk_hz = kPclkHz;
    cfg.timings.h_res = screenWidth;
    cfg.timings.v_res = screenHeight;
    cfg.timings.hsync_pulse_width = 2;
    cfg.timings.hsync_back_porch = 43;
    cfg.timings.hsync_front_porch = 8;
    cfg.timings.vsync_pulse_width = 2;
    cfg.timings.vsync_back_porch = 12;
    cfg.timings.vsync_front_porch = 8;
    cfg.timings.flags.pclk_active_neg = 1;
    cfg.timings.flags.pclk_idle_high = 1;
    cfg.data_width = 16;
    cfg.sram_trans_align = 8;
    cfg.psram_trans_align = 64;
    cfg.hsync_gpio_num = 39;
    cfg.vsync_gpio_num = 40;
    cfg.de_gpio_num = 41;
    cfg.pclk_gpio_num = 42;
    cfg.disp_gpio_num = -1;
    for (int i = 0; i < 16; ++i) {
        cfg.data_gpio_nums[i] = kPinData[i];
    }
    cfg.flags.fb_in_psram = 1;

    if (esp_lcd_new_rgb_panel(&cfg, &s_panel) != ESP_OK) {
        Serial.println("[gui] RGB panel create failed");
        return false;
    }
    if (esp_lcd_panel_reset(s_panel) != ESP_OK || esp_lcd_panel_init(s_panel) != ESP_OK) {
        Serial.println("[gui] RGB panel init failed");
        return false;
    }
    Serial.println("[gui] RGB Sunton 800x480 12MHz 1FB native, no mirror");
    return true;
}

void gui_start()
{
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
    delay(50);

    if (!rgb_panel_init()) {
        return;
    }
    delay(80);
    digitalWrite(2, HIGH);
    touch_init();

    lv_init();

    const size_t buf_px = (size_t)screenWidth * kStripeLines;
    s_stripe = (lv_color_t *)heap_caps_malloc(buf_px * sizeof(lv_color_t),
                                              MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (s_stripe == nullptr) {
        s_stripe = (lv_color_t *)heap_caps_malloc(buf_px * sizeof(lv_color_t),
                                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        Serial.println("[gui] LVGL stripe in PSRAM");
    } else {
        Serial.println("[gui] LVGL stripe in internal RAM");
    }
    if (s_stripe == nullptr) {
        Serial.println("[gui] no draw buffer");
        return;
    }

    lv_disp_draw_buf_init(&draw_buf, s_stripe, nullptr, buf_px);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();
    (void)TAG;
}

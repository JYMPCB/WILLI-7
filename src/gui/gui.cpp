#include <Arduino.h>
#include <Wire.h>
#include <lv_conf.h>
#include <lvgl.h>
#include "gui.h"
#include "../ui/ui.h"
#include "../gfx/LGFX_ESP32S3_RGB_MakerfabsParallelTFTwithTouch70.h"
#include "esp_heap_caps.h" //GPT
#include "../rgb/rgb_gpio.h"
#include "ota_state.h"

static const char* TAG = "gui";

static const uint16_t screenWidth  = 800;
static const uint16_t screenHeight = 480;

static lv_disp_draw_buf_t draw_buf;
// Buffers dinámicos en PSRAM
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;
//static lv_color_t buf[2][ screenWidth * 10 ]; 

LGFX gfx;

static uint8_t g_prev_brightness = 255;
static bool g_bl_was_on = true;

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{

  if(g_ota_active){
    static bool once = false;
    if (!once) {
      g_prev_brightness = 255;   // valor típico
      gfx.setBrightness(0);      // BL OFF
      once = true;
    }
    lv_disp_flush_ready(disp);
    return;
  }

  // --- OTA TERMINÓ: restaurar BL una sola vez ---
  /*static bool was_ota = false;
  if (!was_ota && g_prev_brightness != 0) {
    gfx.setBrightness(g_prev_brightness);
  }
  was_ota = false;*/

  // ---------------- NORMAL ----------------
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;

  if (gfx.getStartCount() == 0) gfx.startWrite();        
  
  gfx.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);        

  if (g_phy_pending) {
    g_phy_pending = false;
    rgb_gpio_set_active_low(g_phy_r, g_phy_g, g_phy_b);
  }

  lv_disp_flush_ready( disp );
}


/*Read the touchpad*/
void my_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
    uint16_t touchX, touchY;

    data->state = LV_INDEV_STATE_REL;

    if( gfx.getTouch( &touchX, &touchY ) )
    {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

/**
 * @brief  
 *
 */
void gui_start(){

  // ----------- GFX -------------
  gfx.begin();
  //  gfx.setColorDepth(16);
  gfx.setBrightness(200);

  lv_init();
  //GPT
  // ✅ Reservar buffers en PSRAM
  const size_t buf_lines = screenHeight; 
  const size_t buf_px = screenWidth * buf_lines;
  const size_t bufSize = buf_px * sizeof(lv_color_t);

  // LVGL buffers en PSRAM (NO internal)
  buf1 = (lv_color_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  buf2 = (lv_color_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  // ✅ INTERNAL DMA CAPABLE (no PSRAM)
  //buf1 = (lv_color_t*)heap_caps_malloc(bufSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  //buf2 = (lv_color_t*)heap_caps_malloc(bufSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);  

  if (!buf1 || !buf2) {
    Serial.println("❌ Error al asignar buffers LVGL en PSRAM");
    Serial.printf("buf1=%p buf2=%p freePSRAM=%u freeHeap=%u\n", buf1, buf2, ESP.getFreePsram(), ESP.getFreeHeap());                        
    return;
  }

  //Serial.printf("[LVGL] buf1=%p buf2=%p\n", buf1, buf2);
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_px); 
  
  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );

  //Serial.printf("[GUI] FreeHeap=%u Min=%u FreePSRAM=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getFreePsram());
  ui_init();
  
}



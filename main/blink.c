#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <stdint.h>
#include <sys/param.h>
// Include FreeRTOS for delay
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_camera.h"
#include "img_converters.h"

#include "../include/define.h"
#include "../include/brain.h"
#include "../include/encoder.h"

#define LED 33 // LED connected to GPIO2

static const char *TAG = "immage comparator";

uint8_t *raw;
uint8_t *sub;
uint8_t *jpg;

int16_t *ordered_dct_Y;
int16_t *ordered_dct_Cb;
int16_t *ordered_dct_Cr;
uint8_t EXT_RAM_BSS_ATTR saved[3*PIX_LEN/16];
area_t EXT_RAM_BSS_ATTR diffDims[20];
int len = 0;
uint8_t different = 0;

static camera_config_t camera_config = {
  .pin_pwdn = PWDN_GPIO_NUM,
  .pin_reset = RESET_GPIO_NUM,
  .pin_xclk = XCLK_GPIO_NUM,
  .pin_sccb_sda = SIOD_GPIO_NUM,
  .pin_sccb_scl = SIOC_GPIO_NUM,
  .pin_d7 = Y9_GPIO_NUM,
  .pin_d6 = Y8_GPIO_NUM,
  .pin_d5 = Y7_GPIO_NUM,
  .pin_d4 = Y6_GPIO_NUM,
  .pin_d3 = Y5_GPIO_NUM,
  .pin_d2 = Y4_GPIO_NUM,
  .pin_d1 = Y3_GPIO_NUM,
  .pin_d0 = Y2_GPIO_NUM,
  .pin_vsync = VSYNC_GPIO_NUM,
  .pin_href = HREF_GPIO_NUM,
  .pin_pclk = PCLK_GPIO_NUM,  
  .xclk_freq_hz = 20000000,        //XCLK 20MHz or 10MHz
  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,
  .pixel_format = PIXFORMAT_JPEG,  //YUV422,GRAYSCALE,RGB565,JPEG
  .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG
  .jpeg_quality = 12,              //0-63 lower number means higher quality
  .fb_count = 1,                   //if more than one, i2s runs in continuous mode. Use only with JPEG
  .grab_mode = CAMERA_GRAB_LATEST,
  .fb_location = CAMERA_FB_IN_PSRAM
};

static esp_err_t init_camera() {
  //initialize the camera
  ESP_LOGI(TAG, "Camera init... ");
  esp_err_t err = esp_camera_init(&camera_config);

  if (err != ESP_OK) {
    //delay(100);  // need a delay here or the next serial o/p gets missed
    ESP_LOGE(TAG, "CRITICAL FAILURE: Camera sensor failed to initialise.");
    ESP_LOGE(TAG, "A full (hard, power off/on) reboot will probably be needed to recover from this.");
    return err;
  } else {
    ESP_LOGI(TAG, "succeeded");

    // Get a reference to the sensor
    sensor_t* s = esp_camera_sensor_get();

    // Dump camera module, warn for unsupported modules.
    switch (s->id.PID) {
      case OV9650_PID: ESP_LOGD(TAG, "WARNING: OV9650 camera module is not properly supported, will fallback to OV2640 operation"); break;
      case OV7725_PID: ESP_LOGD(TAG, "WARNING: OV7725 camera module is not properly supported, will fallback to OV2640 operation"); break;
      case OV2640_PID: ESP_LOGI(TAG, "OV2640 camera module detected"); break;
      case OV3660_PID: ESP_LOGI(TAG, "OV3660 camera module detected"); break;
      default: ESP_LOGD(TAG, "WARNING: Camera module is unknown and not properly supported, will fallback to OV2640 operation");
    }
  }
  return ESP_OK;
}
int app_main() {
    // Configure pin
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "heap_caps_get_total_size(MALLOC_CAP_SPIRAM) = %zu", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    init_camera();
    // gpio_set_level(LED, 0);
    camera_fb_t* fb = esp_camera_fb_get();
    ESP_LOGI(TAG, "taken photo");
    // gpio_set_level(LED, 1);
    ESP_LOGI(TAG, "image size: %zu, %zux%zu", fb->len, fb->width, fb->height);
    raw = (uint8_t*)heap_caps_malloc(3*PIX_LEN, MALLOC_CAP_SPIRAM);
    fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
    esp_camera_fb_return(fb);
    ESP_LOGI(TAG, "post jpg to rgb");
    sub = (uint8_t*)heap_caps_malloc(3*PIX_LEN/16, MALLOC_CAP_SPIRAM);
    subsample(raw, sub);
    store(sub, saved);
    ESP_LOGI(TAG, "post store");
    free(raw);
    free(sub);
    ESP_LOGI(TAG, "--- finished initialization ---");
    // Main loop
    while(true) {
      #ifdef CAM
      setLamp(flashval);
      #endif
      camera_fb_t* fb = esp_camera_fb_get();
      ESP_LOGI(TAG, "image capture"); 
      #ifdef CAM
      setLamp(0);
      #endif
      ESP_LOGI(TAG, "pre conversion");
      raw = (uint8_t*)heap_caps_malloc(3*PIX_LEN, MALLOC_CAP_SPIRAM);
      fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
      ESP_LOGI(TAG, "post conversion");
      esp_camera_fb_return(fb);
      ESP_LOGI(TAG, "pre sub");
      sub = (uint8_t*)heap_caps_malloc(3*PIX_LEN/16, MALLOC_CAP_SPIRAM);
      subsample(raw,sub);
      ESP_LOGI(TAG, "post sub");
      // different = compare(sub, saved, diffDims);
      ESP_LOGI(TAG, "post compare: different = %i", different);
      store(sub, saved);
      ESP_LOGI(TAG, "post save");
      area_t diff = { .x = 0, .y = 0, .w = WIDTH, .h = HEIGHT };
      jpg = (uint8_t*)heap_caps_malloc(3*PIX_LEN, MALLOC_CAP_SPIRAM);
      ESP_LOGI(TAG, "post malloc");
      ordered_dct_Y = (int16_t*)heap_caps_malloc(PIX_LEN, MALLOC_CAP_SPIRAM);
      ordered_dct_Cb = (int16_t*)heap_caps_malloc(PIX_LEN/4, MALLOC_CAP_SPIRAM);
      ordered_dct_Cr = (int16_t*)heap_caps_malloc(PIX_LEN/4, MALLOC_CAP_SPIRAM);
      rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diff);
      len = encodeNsend(jpg, raw, diff);
      ESP_LOGI(TAG, "post encode");
      free(raw);
      free(jpg);
      free(sub);
      // if (different) {
      //   ESP_LOGI(TAG, "Images are different");
      //   for (int i = 0; i < different; i++) {
      //     ESP_LOGI(TAG, "area #%i\nx: %i, y: %i, w: %i, h:%i", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
      //     enlargeAdjust(&diffDims[i]);
      //     ESP_LOGI(TAG, "\t¦\n\tV\nx: %i, y: %i, w: %i, h:%i", diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
      //     jpg = (uint8_t*)heap_caps_malloc(3*diffDims[i].w*diffDims[i].h, MALLOC_CAP_SPIRAM);
      //     len = encodeNsend(jpg, raw, diffDims[i]);
      //     if (len) {
      //       // myBot.sendPhoto(msg, jpg, len);
      //       len = 0;
      //     }
      //     free(jpg);
      //   }
      //   ESP_LOGI(TAG, "Post encodeNsend");
      // } else ESP_LOGI(TAG, "Images are the same");
      // free(raw);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

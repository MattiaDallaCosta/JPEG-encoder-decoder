#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
// Include FreeRTOS for delay
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_spiffs.h"
#include "esp_camera.h"
#include "img_converters.h"

#include "../include/define.h"
#include "../include/brain.h"
#include "../include/encoder.h"

#define LED 33 // LED connected to GPIO2

static const char *TAG = "image comparator";

uint8_t *raw;
uint8_t *sub;
uint8_t *jpg;

int16_t *ordered_dct_Y;
int16_t *ordered_dct_Cb;
int16_t *ordered_dct_Cr;

uint8_t EXT_RAM_BSS_ATTR saved[3*PIX_LEN/16];
area_t EXT_RAM_BSS_ATTR diffDims[20];
huff_code EXT_RAM_BSS_ATTR Luma[2];
huff_code EXT_RAM_BSS_ATTR Chroma[2];
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

esp_vfs_spiffs_conf_t conf = {
  .base_path = "/spiffs",
  .partition_label = NULL,
  .max_files = 5,
  .format_if_mount_failed = false
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
  int i;
  for (i = 0; i < 20; i++) {
    diffDims[i].x = -1;
    diffDims[i].y = -1;
    diffDims[i].w = -1;
    diffDims[i].h = -1;
  }
  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
          ESP_LOGE(TAG, "Failed to mount or format filesystem");
      } else if (ret == ESP_ERR_NOT_FOUND) {
          ESP_LOGE(TAG, "Failed to find SPIFFS partition");
      } else {
          ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
      }
      return 1;
  }
  init_camera();
  camera_fb_t* fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  ESP_LOGI(TAG, "taken photo");
  ESP_LOGI(TAG, "image size: %zu, %zux%zu", fb->len, fb->width, fb->height);
  raw = (uint8_t*)heap_caps_malloc(3*PIX_LEN, MALLOC_CAP_SPIRAM);
  fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
  esp_camera_fb_return(fb);
  ESP_LOGI(TAG, "post jpg to rgb");
  sub = (uint8_t*)heap_caps_malloc(3*PIX_LEN/16, MALLOC_CAP_SPIRAM);
  subsample(raw, sub);
  store(sub, saved);
  // store_file(sub, "/spiffs/saved");
  ESP_LOGI(TAG, "post store");
  free(raw);
  free(sub);
  ESP_LOGI(TAG, "--- finished initialization ---");
  // Main loop
  while(true) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();
    ESP_LOGI(TAG, "image capture"); 
    ESP_LOGI(TAG, "pre decoding");
    raw = (uint8_t*)heap_caps_malloc(3*PIX_LEN, MALLOC_CAP_SPIRAM);
    fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
    ESP_LOGI(TAG, "post decoding");
    esp_camera_fb_return(fb);
    ESP_LOGI(TAG, "pre sub");
    sub = (uint8_t*)heap_caps_malloc(3*PIX_LEN/16, MALLOC_CAP_SPIRAM);
    subsample(raw,sub);
    ESP_LOGI(TAG, "post sub");
    uint8_t offx = 0, offy = 0;
    // for (i = 0; i < PIX_LEN/16; i++) {
    // // ESP_LOGI(TAG, "beginning loop %i", i);
    //   different = compare_block(sub, saved, diffDims, different, offy*(WIDTH/4)+offx);
    //   offx += 16;
    //   if (i%(WIDTH/4)) {
    //     offx = 0;
    //     offy += 16;
    //   }
    // // ESP_LOGI(TAG, "done loop %i : different = %i", i, different);
    // }
    different = compare_block(sub, saved, diffDims, different, offy*(WIDTH/4)+offx);
    ESP_LOGI(TAG, "post compare: different = %i", different);
    store(sub, saved);
    // store_file(sub, "/spiffs/saved");
    free(sub);
    ESP_LOGI(TAG, "post save");
    area_t diff = { .x = 0, .y = 0, .w = WIDTH, .h = HEIGHT };
    jpg = (uint8_t*)heap_caps_malloc(3*diff.h*diff.w, MALLOC_CAP_SPIRAM);
    ordered_dct_Y = (int16_t*)heap_caps_malloc(diff.h*diff.w, MALLOC_CAP_SPIRAM);
    ordered_dct_Cb = (int16_t*)heap_caps_malloc(diff.h*diff.w/4, MALLOC_CAP_SPIRAM);
    ordered_dct_Cr = (int16_t*)heap_caps_malloc(diff.h*diff.w/4, MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "post malloc");
    int j;  
    int last[3] = {0, 0, 0};
    offx = 0;
    offy = 0;
    for (i = 0; i < (diff.w/16)*(diff.h/16); i++) {
      rgb_to_dct_block(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, (offy+diff.y)*WIDTH+(offx+diff.x));
      for (j = 0; j < 4; j++) {
      ordered_dct_Y[((offy+(j%2))*diff.w)*8 + (offx+(j/2))*8] += last[0];
      last[0] += ordered_dct_Y[((offy+(j%2))*diff.w)*8 + (offx+(j/2))*8];
      }
      ordered_dct_Cb[(offy/2)*diff.w+offx/2] -= last[1]; 
      last[1] += ordered_dct_Cb[(offy/2)*diff.w+offx/2]; 
      ordered_dct_Cr[(offy/2)*diff.w+offx/2] -= last[2]; 
      last[2] += ordered_dct_Cr[(offy/2)*diff.w+offx/2]; 
      if(i%(diff.w/16) == (diff.w/16)-1) {
        offx += 16;
        offy = diff.y;
      } else offy += 16;
    }
    ESP_LOGI(TAG, "post dct");
	  init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diff, Luma, Chroma);
    ESP_LOGI(TAG, "post huffman");
	  size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diff, Luma, Chroma);
    ESP_LOGI(TAG, "post encode: size = %zu", size);
    ESP_LOGW(TAG, "last characters: %i, %i", jpg[size - 2], jpg[size - 1]);
    // len = encodeNsend(jpg, raw, diff);
    free(ordered_dct_Y);
    free(ordered_dct_Cb);
    free(ordered_dct_Cr);
    free(raw);
    free(jpg);
    ESP_LOGI(TAG, "post free");
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
  ESP_LOGI(TAG, "--- finished cicle ---\n");
  }
}

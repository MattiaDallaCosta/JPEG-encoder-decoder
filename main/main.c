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
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_camera.h"
#include "img_converters.h"

#include "../include/define.h"
#include "../include/brain.h"
#include "../include/encoder.h"

#define LED 33 // LED connected to GPIO2

static const char *TAG = "image comparator";

static esp_err_t init_camera();
static void photo_callback(void* arg);

// uint8_t *raw;
// uint8_t *sub;

uint8_t EXT_RAM_BSS_ATTR raw[3*PIX_LEN];
uint8_t EXT_RAM_BSS_ATTR sub[3*PIX_LEN/16];

// int16_t *ordered_dct_Y;
// int16_t *ordered_dct_Cb;
// int16_t *ordered_dct_Cr;
// uint8_t *jpg;

int16_t EXT_RAM_BSS_ATTR ordered_dct_Y[PIX_LEN];
int16_t EXT_RAM_BSS_ATTR ordered_dct_Cb[PIX_LEN/4];
int16_t EXT_RAM_BSS_ATTR ordered_dct_Cr[PIX_LEN/4];
uint8_t EXT_RAM_BSS_ATTR jpg[3*PIX_LEN];

uint8_t EXT_RAM_BSS_ATTR saved[3*PIX_LEN/16];
area_t EXT_RAM_BSS_ATTR diffDims[20];
pair_t EXT_RAM_BSS_ATTR differences[2][WIDTH/8];
huff_code EXT_RAM_BSS_ATTR Luma[2];
huff_code EXT_RAM_BSS_ATTR Chroma[2];
uint8_t different = 0;
int timer_val = 1000000;
area_t fullImage = { .x = 0, .y = 0, .w = WIDTH, .h = HEIGHT };
camera_fb_t* fb;

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
  .jpeg_quality = 7,              //0-63 lower number means higher quality
  .fb_count = 1,                   //if more than one, i2s runs in continuous mode. Use only with JPEG
  .grab_mode = CAMERA_GRAB_LATEST,
  .fb_location = CAMERA_FB_IN_PSRAM
};

int app_main() {
  int i;
  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  init_camera();
  fb = esp_camera_fb_get();
  // esp_camera_fb_return(fb);
  // fb = esp_camera_fb_get();
  ESP_LOGI(TAG, "taken photo");
  ESP_LOGI(TAG, "image size: %zu, %zux%zu", fb->len, fb->width, fb->height);
  fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
  esp_camera_fb_return(fb);
  ESP_LOGI(TAG, "post jpg to rgb");
  subsample(raw, sub);
  store(sub, saved);
  ESP_LOGI(TAG, "post store");
  const esp_timer_create_args_t timer_args = {
          .callback = &photo_callback,
          /* name is optional, but may help identify the timer when debugging */
          .name = "periodic"
  };
  esp_timer_handle_t timer;
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer, timer_val));
  ESP_LOGI(TAG, "--- finished initialization ---");
  // Main loop
  while(true) {
    for (i = 0; i < 20; i++) {
      diffDims[i].x = -1;
      diffDims[i].y = -1;
      diffDims[i].w = -1;
      diffDims[i].h = -1;
    }
    for (i = 0; i < WIDTH/8; i++) {
      differences[1][i].beg = differences[0][i].beg = -1;
      differences[1][i].end = differences[0][i].end = -1;
      differences[1][i].row = differences[0][i].row = -1;
      differences[1][i].done = differences[0][i].done = -1;
    }
    different = 0;
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();
    ESP_LOGI(TAG, "image capture"); 
    ESP_LOGI(TAG, "pre decoding");
    fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
    ESP_LOGI(TAG, "post decoding");
    esp_camera_fb_return(fb);
    ESP_LOGI(TAG, "pre sub");
    subsample(raw,sub);
    ESP_LOGI(TAG, "post sub");
    different = compare(sub, saved, diffDims, differences);
    ESP_LOGI(TAG, "post compare: different = %i", different);
    if (different) {
      for (int i = 0; i < different; i++) {
        // gettimeofday(&appo, NULL);
        ESP_LOGI(TAG, "pre [%i] {%i, %i, %i, %i}", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
        enlargeAdjust(diffDims+i);
        ESP_LOGI(TAG, "post [%i] {%i, %i, %i, %i}", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
        rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cb, diffDims[i]);
	      init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
	      size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
        // gettimeofday(&op_t, NULL);
        // millielapsed = (op_t.tv_usec - appo.tv_usec)/1000;
        // secelapsed = (op_t.tv_sec - appo.tv_sec);
        // printf("conversion time for diff #%i:          %li:%li:%li.%s%li\n", i, (secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
      }
    } else {
      rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cb, fullImage);
      ESP_LOGI(TAG, "post dct");
	    init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
      ESP_LOGI(TAG, "post huffman");
	    size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
      ESP_LOGI(TAG, "post encode: size = %zu", size);
      ESP_LOGW(TAG, "last characters: %i, %i", jpg[size - 2], jpg[size - 1]);
      ESP_LOGI(TAG, "post free");
    }
    store(sub, saved);
    ESP_LOGI(TAG, "post save");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "--- finished cicle ---\n");
  }
}

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

static void photo_callback(void *arg) {
  int i;
  for (i = 0; i < 20; i++) {
    diffDims[i].x = -1;
    diffDims[i].y = -1;
    diffDims[i].w = -1;
    diffDims[i].h = -1;
  }
  for (i = 0; i < WIDTH/8; i++) {
    differences[1][i].beg = differences[0][i].beg = -1;
    differences[1][i].end = differences[0][i].end = -1;
    differences[1][i].row = differences[0][i].row = -1;
    differences[1][i].done = differences[0][i].done = -1;
  }
  different = 0;
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  ESP_LOGI(TAG, "image capture"); 
  ESP_LOGI(TAG, "pre decoding");
  fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
  ESP_LOGI(TAG, "post decoding");
  esp_camera_fb_return(fb);
  ESP_LOGI(TAG, "pre sub");
  subsample(raw,sub);
  ESP_LOGI(TAG, "post sub");
  different = compare(sub, saved, diffDims, differences);
  ESP_LOGI(TAG, "post compare: different = %i", different);
  if (different) {
    for (int i = 0; i < different; i++) {
      // gettimeofday(&appo, NULL);
      ESP_LOGI(TAG, "pre [%i] {%i, %i, %i, %i}", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
      enlargeAdjust(diffDims+i);
      ESP_LOGI(TAG, "post [%i] {%i, %i, %i, %i}", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
      rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cb, diffDims[i]);
	    init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
	    size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
      // gettimeofday(&op_t, NULL);
      // millielapsed = (op_t.tv_usec - appo.tv_usec)/1000;
      // secelapsed = (op_t.tv_sec - appo.tv_sec);
      // printf("conversion time for diff #%i:          %li:%li:%li.%s%li\n", i, (secelapsed/3600)%60, (secelapsed/60)%60, (secelapsed)%60, ((millielapsed)%1000) > 99 ? "" : (((millielapsed)%1000) > 9 ? "0" : "00"), (millielapsed)%1000);
    }
  } else {
    rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cb, fullImage);
    ESP_LOGI(TAG, "post dct");
	  init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
    ESP_LOGI(TAG, "post huffman");
	  size_t size = write_jpg(jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, fullImage, Luma, Chroma);
    ESP_LOGI(TAG, "post encode: size = %zu", size);
    ESP_LOGW(TAG, "last characters: %i, %i", jpg[size - 2], jpg[size - 1]);
    ESP_LOGI(TAG, "post free");
  }
  store(sub, saved);
  ESP_LOGI(TAG, "post save");
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "--- finished cicle ---\n");
}

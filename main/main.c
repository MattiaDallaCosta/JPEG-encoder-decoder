#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_camera.h"
#include "img_converters.h"

#include "../include/define.h"
#include "../include/brain.h"
#include "../include/encoder.h"

static const char *TAG = "image comparator";

#define MOUNT_POINT "/sdcard"

const char *sub_file = MOUNT_POINT"/sub";
const char *store_file = MOUNT_POINT"/stored";
const char *j_file = MOUNT_POINT"/jpg";
const char *Y_file = MOUNT_POINT"/Y";
const char *Cb_file = MOUNT_POINT"/Cb";
const char *Cr_file = MOUNT_POINT"/Cr";
const char *info_file = MOUNT_POINT"/info";

static esp_err_t init_camera();

uint8_t EXT_RAM_BSS_ATTR raw[3*PIX_LEN];
uint8_t EXT_RAM_BSS_ATTR sub[3*PIX_LEN/16];

int16_t EXT_RAM_BSS_ATTR ordered_dct_Y[PIX_LEN];
int16_t EXT_RAM_BSS_ATTR ordered_dct_Cb[PIX_LEN/4];
int16_t EXT_RAM_BSS_ATTR ordered_dct_Cr[PIX_LEN/4];
uint8_t EXT_RAM_BSS_ATTR jpg[3*PIX_LEN];

uint8_t EXT_RAM_BSS_ATTR saved[3*PIX_LEN/16];
area_t EXT_RAM_BSS_ATTR diffDims[100];
pair_t EXT_RAM_BSS_ATTR differences[2][WIDTH/8];
huff_code EXT_RAM_BSS_ATTR Luma[2];
huff_code EXT_RAM_BSS_ATTR Chroma[2];
uint8_t different = 0;
int sleep_val = 1000000;
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

esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};

spi_bus_config_t bus_cfg = {
    .mosi_io_num = PIN_NUM_MOSI,
    .miso_io_num = PIN_NUM_MISO,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
};

void app_main(void) {
  sdmmc_card_t *card;
  const char mount_point[] = MOUNT_POINT;
  ESP_LOGI(TAG, "Initializing SD card");
  ESP_LOGI(TAG, "Using SPI peripheral");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize bus.");
      return;
  }
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = PIN_NUM_CS;
  slot_config.host_id = host.slot;

  ESP_LOGI(TAG, "Mounting filesystem");
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
          ESP_LOGE(TAG, "Failed to mount filesystem. "
                   "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
      } else {
          ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                   "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
      }
      return;
  }
  ESP_LOGI(TAG, "Filesystem mounted");

  sdmmc_card_print_info(stdout, card);
  // camera
  int i = 0;
  init_camera();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);
  FILE* sub_f = fopen(store_file, "w");
  subsample(sub_f, raw, sub);
  store(sub, saved);
  fclose(sub_f);
  ESP_LOGI(TAG, "--- finished initialization ---");
  while (1) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();
    fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, raw);
    esp_camera_fb_return(fb);
    sub_f = fopen(sub_file, "w");
    subsample(sub_f, raw, sub);
    fclose(sub_f);
    int different = compare(sub, saved, diffDims, differences);
    if(different) {
      ESP_LOGI(TAG, "Images are different [#differences = %i]%s", different, sleep_val == 10000 ? "\nsleep timer set to 1s" : "");
      for (i = 0; i < different; i++) {
        ESP_LOGI(TAG, "diffDims[%i] = {%i, %i, %i, %i}", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
        rgb_to_dct(raw, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i]);
	      init_huffman(ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
        char jpg_file[1024], end[10];
        strcpy(jpg_file, j_file);
        sprintf(end, "-%i", i);
        strcat(jpg_file, end);
        ESP_LOGI(TAG, "name for jpg = %s", jpg_file);
        FILE * jpg_f = fopen(jpg_file, "w");
	      size_t size = write_jpg(jpg_f, jpg, ordered_dct_Y, ordered_dct_Cb, ordered_dct_Cr, diffDims[i], Luma, Chroma);
        fclose(jpg_f);
      sleep_val = 1000;
      }
    } else {
      ESP_LOGI(TAG, "Images are the same%s", sleep_val == 1000 ? "\nsleep timer set to 10s" : "");
      sleep_val = 10000;
    }
    struct stat st;
    if(!stat(store_file, &st)) unlink(store_file);
    store(sub, saved);
    if (rename(sub_file, store_file)) ESP_LOGE(TAG, "Rename failed");
    vTaskDelay(sleep_val / portTICK_PERIOD_MS);
  }
    ESP_LOGI(TAG, "done");
    return;
}

static esp_err_t init_camera() {
  //initialize the camera
  ESP_LOGI(TAG, "Camera init... ");
  esp_err_t err = esp_camera_init(&camera_config);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "CRITICAL FAILURE: Camera sensor failed to initialise.");
    ESP_LOGE(TAG, "A full (hard, power off/on) reboot will probably be needed to recover from this.");
    return err;
  } else {

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

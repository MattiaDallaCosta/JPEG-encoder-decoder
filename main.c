#include <stdint.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
// Include FreeRTOS for delay

#include "include/define.h"
#include "include/brain.h"
#include "include/encoder.h"

#define LED 33 // LED connected to GPIO2

static const char *TAG = "image comparator";

uint8_t *raw;
uint8_t *sub;
uint8_t *jpg;

int16_t *ordered_dct_Y;
int16_t *ordered_dct_Cb;
int16_t *ordered_dct_Cr;
// area_t * diffDims;

uint8_t saved[3*PIX_LEN/16];
area_t diffDims[20];
pair_t differences[100];
huff_code Luma[2];
huff_code Chroma[2];
int len = 0;
uint8_t different = 0;

int app_main() {
  int i;
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
  ESP_LOGI(TAG, "post store");
  free(raw);
  free(sub);
  ESP_LOGI(TAG, "--- finished initialization ---");
  // Main loop
  while(true) {
    for (i = 0; i < 20; i++) {
      diffDims[i].x = -1;
      diffDims[i].y = -1;
      diffDims[i].w = -1;
      diffDims[i].h = -1;
    }
    different = 0;
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
    for (i = 0; i < PIX_LEN/256; i++) {
    // ESP_LOGI(TAG, "offx = %i, offy = %i", offx, offy);
      different = compare_block(sub, saved, diffDims, differences, different, offy*(WIDTH/4)+offx);
      if (!(i%(WIDTH/16)) && i) {
        offx = 0;
        offy += 4;

      } else offx += 4;
    // ESP_LOGI(TAG, "done loop %i : different = %i", i, different);
    if(different > 19) break;
    }
    ESP_LOGI(TAG, "post compare: different = %i", different);
    for (int i = 0; i < different; i++) {
      enlargeAdjust(diffDims+i);
      ESP_LOGI(TAG, "diffDims[%i]: .x = %i, .y = %i, .w = %i, .h = %i", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
    }
    store(sub, saved);
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
   //  free(jpg);
    free(raw);
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

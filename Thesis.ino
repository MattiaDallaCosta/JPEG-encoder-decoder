#include "esp_camera.h"
extern "C" {
#include "include/define.h"
#include "include/encoder.h"
#include "include/brain.h"
#include "include/structs.h"
}
//uint8_t raw[3][PIX_LEN];
uint8_t subsampled[3][PIX_LEN/16];
int state = LOW;
char text[100];
char savedName[100];
char newname[100];
area_t diffDims[20];

void setup() {
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  pinMode(33, OUTPUT);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_1;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_RGB565;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if(!psramFound()) {
      Serial.println("\r\nFatal Error; Halting");
      Serial.println("No PSRAM found; camera cannot be initialised: Please check the board config for your module.");
      return;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  } 
  Serial.printf("Camera initialized succesfully");
  sensor_t * s = esp_camera_sensor_get();

}

void loop() {
  Serial.printf("BenDio 1\n");
  camera_fb_t *fb = esp_camera_fb_get();
    Serial.printf("BenDio 2\n");
    // put image into raw
  subsample(fb->buf, subsampled);
  Serial.printf("BenDio 3\n");
  uint8_t different = compare(subsampled, diffDims);
  Serial.printf("BenDio 4\n");
  Serial.printf("different %i\n", different);
  if (different) {
      printf("Images are different\n");
      for (int i = 0; i < different; i++) {
        getName(text, newname, i);
        Serial.printf("area #%i\nx: %i, y: %i, w: %i, h:%i\n", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
        enlargeAdjust(&diffDims[i]);
        Serial.printf("\t¦\n\tV\nx: %i, y: %i, w: %i, h:%i\n", i, diffDims[i].x, diffDims[i].y, diffDims[i].w, diffDims[i].h);
        encodeNsend(newname, fb->buf, diffDims[i]);
      }
      Serial.printf("Post encodeNsend\n");
    } else Serial.printf("Images are the same\n");
    store(subsampled);    
    delay(100);
    state = state == LOW ? HIGH : LOW;
    digitalWrite(33, state);
  Serial.printf("BenDio 2\n");
}

/* SPDX-License-Identifier: MIT */

#include "esp_camera.h"
#include <WiFi.h>


//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"
#include "cfg.h"

const char* ssid = SSID;
const char* password = SSID_PW;
char baseMacChr[18] = {0};
String macStr;
int failCnt = 0;

esp_err_t updateImage(String macStr);

static void getMACAddress(char *baseMacChr) {
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  sprintf(baseMacChr, "%02X-%02X-%02X-%02X-%02X-%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
}

static esp_err_t setupCam() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
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
  config.pixel_format = PIXFORMAT_JPEG;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    Serial.println("Have PSRAM, using UXGA and Q=10");
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("No PSRAM, using SVGA and Q=12");
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return ESP_FAIL;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  // s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_framesize(s, FRAMESIZE_UXGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  return ESP_OK;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  getMACAddress(baseMacChr);
  macStr = String(baseMacChr);

  Serial.printf("MAC address: %s\n", baseMacChr);

  esp_err_t err = setupCam();
  if (err != ESP_OK) {
    Serial.printf("Camera init failed, exiting", err);
    return;
  }

  Serial.printf("Connecting to Wifi with SSID=%s\n", ssid);

  WiFi.begin(ssid, password);

  failCnt = 0;
  Serial.println("Waiting for connect, '.' ~ 0.5sec");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    failCnt++;
    if (failCnt > 10 * 60 * 1000 / 500) { // 10min = 10*60*1000/500
      Serial.println("too many failures, restarting");
      failCnt = 0;
      ESP.restart();
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Camera Ready, local IP = ");
  Serial.println(WiFi.localIP());
  failCnt = 0;
}

void loop() {
  esp_err_t err = updateImage(macStr);
  if (err != ESP_OK) {
    Serial.printf("updateImage error, failCnt=%d\r\n", failCnt);
    failCnt++;
    if (failCnt > 10 * 60 * 1000 / 30000) { // 10min = 10*60*1000/(30*1000)
      Serial.println("too many failures, restarting");
      failCnt = 0;
      ESP.restart();
    }
  } else {
    failCnt = 0;
  }
  delay(30000); // every 30 seconds
}

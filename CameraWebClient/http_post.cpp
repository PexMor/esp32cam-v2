/* SPDX-License-Identifier: MIT */

#include <WiFiClient.h>
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include "cfg.h"

char *request_content = "--" MIME_BOUNDARY "\r\n"
                        "Content-Disposition: form-data; name=\"file\"; filename=\"%s.jpg\"\r\n"
                        "Content-Type: image/jpeg\r\n\r\n";
char *request_end = "\r\n" "--" MIME_BOUNDARY "--\r\n";


// helper function to get an image from camera, and then upload it as
// http post mime body
esp_err_t updateImage( String macStr) {
  WiFiClient client;

  char status[64] = {0};

  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  int64_t fr_start = esp_timer_get_time();

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return ESP_FAIL;
  }

  size_t fb_len = 0;
  if (fb->format != PIXFORMAT_JPEG) {
    Serial.printf("Unexpected frame format id=%d\n", fb->format);
    return ESP_FAIL;
  }
  fb_len = fb->len;
  //res = httpd_resp_send(req, (const char *)fb->buf, fb->len);

  char jpgMimeHeader[1234];
  snprintf(jpgMimeHeader, sizeof(jpgMimeHeader), request_content, "red");//String(millis()).c_str());
  uint32_t content_len = fb_len + strlen(jpgMimeHeader) + strlen(request_end);
  Serial.printf("sizes:fb=%d,jpgMimeHeader=%d,req_end=%d\n", fb_len, strlen(jpgMimeHeader), strlen(request_end));

  ////////////////////////////////////////
  String request = "POST " HTTP_PATH "?mac=" + macStr + "&id=" ID " HTTP/1.1\r\n";
  request += "Host: " HTTP_HOST "\r\n";
  request += "User-Agent: esp32cam/" VER ")\r\n";
  request += "X-Esp32-Mac: " + macStr + "\r\n";
  request += "Accept: */*\r\n";
  request += "Content-Length: " + String(content_len) + "\r\n";
  request += "Content-Type: multipart/form-data; boundary=" MIME_BOUNDARY "\r\n";
  request += "Expect: 100-continue\r\n";
  request += "\r\n";

  Serial.print(request);
  if (!client.connect(HTTP_HOST, HTTP_PORT)) {
    Serial.println("Connection failed");
    return ESP_FAIL;
  }
  client.print(request);
  Serial.println("running read bytes until");
  //delay(200);

  client.readBytesUntil('\r', status, sizeof(status));
  Serial.printf("status:%s\n", status);
  int status_cmp = strncmp(status, "HTTP/1.1 100 Continue", sizeof(status));
  if ( status_cmp != 0) {
    Serial.printf("Unexpected response (%d): '%s'\n", status_cmp, status);
    client.stop();
    return ESP_FAIL;
  }

  Serial.printf("##\n%s\n##\n", jpgMimeHeader);
  client.print(jpgMimeHeader);

  Serial.printf("%02x %02x %02x %02x %02x %02x %02x %02x\n",
                fb->buf[0], fb->buf[1], fb->buf[2], fb->buf[3],
                fb->buf[4], fb->buf[5], fb->buf[6], fb->buf[7]);
  uint8_t *image = fb->buf;
  size_t size = fb_len;
  size_t offset = 0;
  size_t ret = 0;
  while (size > 0) {
    Serial.printf("B:o:% 5d s:% 5d\n", offset, size);
    ret = client.write(image + offset, size);
    Serial.printf("A:o:% 5d s:% 5d,r:% 5d\n", offset, size, ret);
    offset += ret;
    size -= ret;
  }
  Serial.printf("E:o:% 5d s:% 5d\n", offset, size);
  client.print(request_end);
  client.find("\r\n");

  bzero(status, sizeof(status));
  client.readBytesUntil('\r', status, sizeof(status));
  Serial.print("Response: ");
  Serial.println(status);
  bzero(status, sizeof(status));
  client.readBytesUntil('\r', status, sizeof(status));
  Serial.print("Response: ");
  Serial.println(status);
  Serial.println("Wait for double cr+lf");
  if (!client.find("\r\n\r\n")) {
    Serial.println("Invalid response");
  }
  bzero(status, sizeof(status));
  client.readBytesUntil('\r', status, sizeof(status));
  Serial.print("Zero: ");
  Serial.println(status);

  client.flush();
  client.stop();
  ////////////////////////////////////////

  esp_camera_fb_return(fb);
  int64_t fr_end = esp_timer_get_time();
  Serial.printf("JPG: %u B %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start) / 1000));
  res = ESP_OK;
  return res;
}

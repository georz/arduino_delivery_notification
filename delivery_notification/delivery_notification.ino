#include <Arduino.h>
#include <WiFi.h>
#include "battery.h"
#include "esp_camera.h"
#include "camera_pins.h"
#include "esp_http_server.h"

const char *WIFI_SSID = "-- SSID --";
const char *WIFI_PASSWORD = "-- PASSWORD --";

const unsigned int DELAY_WIFI_CONNECT = 1000;
const unsigned int INTERVAL_WIFI_RECONNECT = 15000;
const unsigned int MAX_RETRY_WIFI_RECONNECT = 5;

const unsigned int DELAY_LOOP = 60000;

const unsigned int PREPARE_CAPTURE_COUNT = 3;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //Serial.println("Connecting to WiFi");

  unsigned long previousMillis = millis();
  unsigned int retryCount = 0;
  bool isConnecting = true;

  while (isConnecting) {
    //Serial.print(". ");
    delay(DELAY_WIFI_CONNECT);

    if ((WiFi.status() == WL_CONNECTED)) {
      isConnecting = false;
    } else {
      unsigned long currentMillis = millis();
      if ((currentMillis - previousMillis >= INTERVAL_WIFI_RECONNECT)) {
        if (retryCount >= MAX_RETRY_WIFI_RECONNECT) {
          //Serial.println("ESP.restart");
          ESP.restart();
        }
        WiFi.disconnect();
        WiFi.reconnect();
        retryCount += 1;
        previousMillis = currentMillis;
        //Serial.printf("Reconnecting to WiFi [retryCount: %d]\n", retryCount);
      }
    }
  }
  //Serial.println(WiFi.localIP());
}

esp_err_t prepareCapture() {
  int i = 0;
  while (i < PREPARE_CAPTURE_COUNT) {
    camera_fb_t *fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    i++;
  }  
}

esp_err_t getCapture(httpd_req_t *req) {
  esp_err_t resp = ESP_OK;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  //Serial.printf("fb->len: %d\n", fb->len);

  resp = httpd_resp_set_type(req, "image/jpeg");
  if (resp == ESP_OK) {
    resp = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    resp = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  }
  esp_camera_fb_return(fb);

  return resp;
}

httpd_handle_t startWebServer(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;

  httpd_uri_t uriGetCapture = {
    .uri      = "/capture",
    .method   = HTTP_GET,
    .handler  = getCapture,
    .user_ctx = NULL
  };

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &uriGetCapture);
  }
  return server;
}

bool initCamera(void) {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_XGA;
  config.jpeg_quality = 10;
  config.fb_count     = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  //s->set_hmirror(s, 1);
  //s->set_brightness(s, 0);
  //s->set_saturation(s, 0);
  //s->set_whitebal(s, 0);
  //s->set_awb_gain(s, 0);

  return true;
}

void setup() {
  //Serial.begin(115200);

  bat_init();
  bat_disable_output();

  connectWiFi();
  initCamera();
  startWebServer();

  // 予備撮影（最初の数枚に不具合が発生することがあるため）
  prepareCapture();
}

void loop() {
  if ((WiFi.status() != WL_CONNECTED)) {
    //Serial.println("Reconnecting to WiFi");
    WiFi.disconnect();
    connectWiFi();
  }
  delay(DELAY_LOOP);
}
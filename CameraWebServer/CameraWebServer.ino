#include "esp_camera.h"
#include <WiFi.h>

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const char *ssid = "TP-Link_5944";
const char *password = "21002373";

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Print reset reason for debugging
  Serial.printf("Reset reason: %d\n", esp_reset_reason());
  Serial.printf("CPU Freq: %d MHz\n", getCpuFrequencyMhz());
  
  // Initialize relay pin
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

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
  config.xclk_freq_hz = 20000000;  // Reduced from 20MHz for stability
  config.pixel_format = PIXFORMAT_JPEG;
  
  // More conservative settings to prevent resets
  if(psramFound()){
    Serial.println("PSRAM found");
    config.frame_size = FRAMESIZE_QVGA;     // Changed from UXGA - more stable
    config.jpeg_quality = 12;              // Lower quality = less memory
    config.fb_count = 1;                   // Use 2 buffers for smoother streaming
  } else {
    Serial.println("PSRAM NOT found - limited functionality");
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();
  
  // Initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);       // flip it back
    s->set_brightness(s, 1);  // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  
  // Keep VGA for face detection, don't override to QVGA
  // s->set_framesize(s, FRAMESIZE_QVGA);  // REMOVED

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  // WiFi configuration for stability
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // Disable WiFi sleep
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed! Restarting...");
    delay(1000);
    ESP.restart();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Print memory status before starting server
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("Min free heap: %d bytes\n", ESP.getMinFreeHeap());

  startCameraServer();

  Serial.print("\nCamera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  // Monitor memory and WiFi status
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {  // Every 10 seconds
    lastCheck = millis();
    Serial.printf("Free heap: %d, Min free: %d, PSRAM: %d\n", 
                  ESP.getFreeHeap(), 
                  ESP.getMinFreeHeap(),
                  ESP.getFreePsram());
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Reconnecting...");
      WiFi.reconnect();
    }
  }
  
  delay(100);  // Short delay to prevent watchdog issues
}

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>       // ไลบรารีจัดการ Wi-Fi
#include <HTTPUpdate.h>        // ไลบรารีสำหรับทำ OTA Update
#include <ArduinoJson.h>       // ไลบรารีอ่านไฟล์ JSON

// --- กำหนดเวอร์ชันปัจจุบันของเฟิร์มแวร์ ---
#define CURRENT_VERSION "1.0.0"

// --- URL ของไฟล์ JSON บน Server ---
const char* firmwareJsonUrl = "http://172.24.150.112/version.json";

// --- LINE Token และ Target ID ---
String lineToken = "LlV1kRa2fOXd+gS/1M8w54fBmBdwqUBxPMpM1XdIN8OrsOCoDhCq/BD0HJopBFvoJykALqbxbppMmH8YsdxFHI0CcnnAs+tmx8VjPaIAxKPiz8EXx0S/kHl4cCGbbH9uBNAnuGA3aOR4V7890iee9wdB04t89/1O/w1cDnyilFU=";
String targetId  = "U223c6efb7d1a00291a50805903753a9c";

int a2Count = 0;
bool triggerLineFlag = false;

// ฟังก์ชันสำหรับส่ง LINE Message เฉพาะการแจ้งเตือน Sensor
void sendLineMessage(String text) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, "https://api.line.me/v2/bot/message/push");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + lineToken);

    String jsonPayload = "{\"to\":\"" + targetId + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + text + "\"}]}";

    int httpCode = http.POST(jsonPayload);
    if (httpCode == 200) {
      Serial.println("[LINE] Send notification successful!");
    } else {
      Serial.printf("[LINE] Failed, HTTP Code: %d\n", httpCode);
    }
    http.end();
  }
}

// ฟังก์ชันตรวจสอบและอัปเดตเฟิร์มแวร์อัตโนมัติ (แจ้งเตือนผ่าน Serial Monitor)
void checkAndDoOTA() {
  Serial.println("[OTA] Checking for firmware updates...");
  
  WiFiClient client;
  HTTPClient http;

  http.begin(client, firmwareJsonUrl);
  http.setTimeout(10000); // ตั้งเวลารอการตอบรับ 10 วินาที

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      String remoteVersion = doc["version"].as<String>();
      String binUrl = doc["url"].as<String>();

      Serial.printf("[OTA] Current Version: %s | Remote Version: %s\n", CURRENT_VERSION, remoteVersion.c_str());

      // เปรียบเทียบเวอร์ชัน
      if (remoteVersion != CURRENT_VERSION) {
        Serial.println("[OTA] New version found! Starting update...");

        // ปิด Connection เดิมคืน RAM ให้ระบบก่อนดาวน์โหลด
        http.end(); 

        WiFiClient otaClient;
        t_httpUpdate_return ret = httpUpdate.update(otaClient, binUrl);

        switch (ret) {
          case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] Update failed. Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;
            
          case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] No updates available.");
            break;

          case HTTP_UPDATE_OK:
            Serial.println("[OTA] Update successful! Rebooting...");
            break;
        }
        return;
      } else {
        Serial.println("[OTA] Firmware is up to date.");
      }
    } else {
      Serial.println("[OTA] Failed to parse JSON configuration.");
    }
  } else {
    Serial.printf("[OTA] Failed to fetch JSON, HTTP Code: %d\n", httpCode);
  }
  http.end();
}

// Task สำหรับส่งค่า Sensor
void TaskSensorCode(void * pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(1000); 
  int counter = 0;
  xLastWakeTime = xTaskGetTickCount();

  while(1) {
    counter++;

    if (counter % 2 == 0) {
      Serial.println("A1");
    }

    if (counter % 5 == 0) {
      Serial.println("A2");
      a2Count++;
      Serial.printf("-> A2 Count: %d/5\n", a2Count);

      if (a2Count >= 5) {
        Serial.println("-> Trigger LINE: ส่ง A2 ครบ 5 ครั้งแล้ว!");
        triggerLineFlag = true;
        a2Count = 0; 
      }
    }

    if (counter >= 100) {
      counter = 0;
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. ระบบ WiFiManager
  WiFiManager wm;
  Serial.println("[WiFi] Connecting...");
  bool res = wm.autoConnect("ESP32_Config", "12345678");

  if (!res) {
    Serial.println("[WiFi] Failed to connect or hit timeout. Rebooting...");
    ESP.restart();
  }

  Serial.println("[WiFi] Connected successfully!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 2. ตรวจสอบและอัปเดต Firmware (OTA)
  checkAndDoOTA();

  // 3. สร้าง FreeRTOS Task สำหรับส่งค่า Sensor
  xTaskCreate(
    TaskSensorCode,    
    "TaskSensor",      
    8192,              
    NULL,              
    1,                 
    NULL  
  );
}

void loop() {
  if (triggerLineFlag) {
    triggerLineFlag = false; 
    sendLineMessage("Alert: ส่ง A2 ครบ 5 ครั้งแล้วครับ!");
  }
  
  vTaskDelay(pdMS_TO_TICKS(100));
}
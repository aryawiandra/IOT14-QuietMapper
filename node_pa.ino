// ==========================================
// NODE - RTOS NO-I2C (TIMEOUT FIXED)
// ==========================================
#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>

// --- CONFIG MESH ---
#define MESH_PREFIX     "MESH_BARU_FIX"
#define MESH_PASSWORD   "12345678"
#define MESH_PORT       7777

// --- PINS ---
#define PIN_MIC 34
#define PIN_LED_R 25
#define PIN_LED_G 26
#define PIN_LED_B 27

// --- RTC MEMORY ---
RTC_DATA_ATTR bool isWorkMode = true;

// --- OBJECTS ---
painlessMesh mesh;

// --- GLOBAL VARS ---
unsigned long wakeUpTime = 0;
TaskHandle_t TaskMeshHandle;
TaskHandle_t TaskMainHandle;

// --- HELPER FUNCTIONS ---
void setRGB(int r, int g, int b) {
  analogWrite(PIN_LED_R, 255 - r);
  analogWrite(PIN_LED_G, 255 - g);
  analogWrite(PIN_LED_B, 255 - b);
}

void goToSleep(int duration) {
  Serial.println(">>> GOING TO SLEEP...");
  setRGB(0, 0, 0);
  
  // Clean up Tasks
  if(TaskMeshHandle != NULL) vTaskDelete(TaskMeshHandle);
  if(TaskMainHandle != NULL) vTaskDelete(TaskMainHandle);

  esp_sleep_enable_timer_wakeup(duration * 1000000ULL);
  esp_deep_sleep_start();
}

void receivedCallback(uint32_t from, String &msg) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, msg);

  if (doc.containsKey("cmd")) {
    String cmd = doc["cmd"];
    
    // CASE 1: SLEEP COMMAND
    if (cmd == "SLEEP" && isWorkMode) {
      Serial.println("CMD: SLEEP received.");
      isWorkMode = false;
      goToSleep(10); 
    } 
    // CASE 2: WAKE COMMAND (Critical!)
    else if (cmd == "WAKE" && !isWorkMode) {
      Serial.println("CMD: WAKE received. Restarting...");
      isWorkMode = true;
      delay(100); 
      ESP.restart(); // Restart ensures clean state for Work Mode
    }
  }
}

// ==========================================
// RTOS TASK 1: MESH (Priority 2)
// ==========================================
void TaskMesh(void *pvParameters) {
  for (;;) {
    mesh.update();
    vTaskDelay(1 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// RTOS TASK 2: SENSOR & LOGIC (Priority 1)
// ==========================================
void TaskMain(void *pvParameters) {
  for (;;) {
    
    // --- MODE: ACTIVE WORK ---
    if (isWorkMode) {
      long sum = 0;
      for (int i = 0; i < 30; i++) sum += analogRead(PIN_MIC);
      int noise = sum / 30;

      if (noise > 2000) setRGB(255, 0, 0); 
      else setRGB(0, 255, 0); 

      DynamicJsonDocument doc(1024);
      doc["node"] = mesh.getNodeId();
      doc["noise"] = noise;
      String msg;
      serializeJson(doc, msg);
      mesh.sendBroadcast(msg);

      Serial.printf("WORK MODE: Noise %d sent.\n", noise);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } 
    
    // --- MODE: SILENT CHECK (Listening) ---
    else {
      // Blink faint blue to indicate "Listening"
      setRGB(0, 0, 50); vTaskDelay(50/portTICK_PERIOD_MS); setRGB(0, 0, 0);
      Serial.print("."); 

      // TIMEOUT FIX: Increased to 30 Seconds!
      // This ensures we wait long enough for the Root Beacon (1.5s interval)
      if (millis() - wakeUpTime > 30000) {
        Serial.println("\nTIMEOUT: No 'WAKE' command. Sleeping again.");
        goToSleep(10);
      }
      
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  wakeUpTime = millis();

  // Manual Power On -> Force Work Mode
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    isWorkMode = true; 
  }

  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  setRGB(0,0,0);

  // Mesh Init (Setup context)
  mesh.setDebugMsgTypes(ERROR); 
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  Serial.println(isWorkMode ? ">>> START: WORK MODE" : ">>> START: SILENT LISTENING");

  xTaskCreatePinnedToCore(TaskMesh, "MeshTask", 5000, NULL, 2, &TaskMeshHandle, 1);
  xTaskCreatePinnedToCore(TaskMain, "MainTask", 5000, NULL, 1, &TaskMainHandle, 1);
}

void loop() {
  vTaskDelete(NULL);
}
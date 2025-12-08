// ==========================================
// NODE - RTOS NO-I2C (LIGHTWEIGHT)
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
// Saves state during Deep Sleep
RTC_DATA_ATTR bool isWorkMode = true;

// --- OBJECTS ---
painlessMesh mesh;

// --- GLOBAL VARS ---
unsigned long wakeUpTime = 0;

// --- TASK HANDLES ---
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
  setRGB(0, 0, 0); // Turn off LEDs
  
  // Clean up Tasks before sleeping
  if(TaskMeshHandle != NULL) vTaskDelete(TaskMeshHandle);
  if(TaskMainHandle != NULL) vTaskDelete(TaskMainHandle);

  // Setup Deep Sleep
  esp_sleep_enable_timer_wakeup(duration * 1000000ULL);
  esp_deep_sleep_start();
}

// --- MESH CALLBACK ---
void receivedCallback(uint32_t from, String &msg) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, msg);

  if (doc.containsKey("cmd")) {
    String cmd = doc["cmd"];
    
    // CASE 1: COMMAND TO SLEEP
    if (cmd == "SLEEP") {
      Serial.println("CMD: SLEEP received.");
      isWorkMode = false;
      goToSleep(10); // Sleep 10 seconds
    } 
    // CASE 2: COMMAND TO WAKE
    else if (cmd == "WAKE" && !isWorkMode) {
      Serial.println("CMD: WAKE received. Restarting...");
      isWorkMode = true;
      ESP.restart(); // Restart to fresh start
    }
  }
}

// ==========================================
// RTOS TASK 1: MESH KEEPALIVE
// ==========================================
void TaskMesh(void *pvParameters) {
  // This task only updates the mesh
  for (;;) {
    mesh.update();
    vTaskDelay(1 / portTICK_PERIOD_MS); // Yield to other tasks
  }
}

// ==========================================
// RTOS TASK 2: SENSOR & LOGIC
// ==========================================
void TaskMain(void *pvParameters) {
  for (;;) {
    
    // --- MODE: ACTIVE WORK ---
    if (isWorkMode) {
      // 1. Read Mic
      long sum = 0;
      for (int i = 0; i < 30; i++) sum += analogRead(PIN_MIC);
      int noise = sum / 30;

      // 2. LED Feedback
      if (noise > 2000) setRGB(255, 0, 0); // Red (High Noise)
      else setRGB(0, 255, 0); // Green (Normal)

      // 3. Send Data to Mesh
      DynamicJsonDocument doc(1024);
      doc["node"] = mesh.getNodeId();
      doc["noise"] = noise;
      String msg;
      serializeJson(doc, msg);
      mesh.sendBroadcast(msg);

      Serial.printf("WORK MODE: Noise %d sent.\n", noise);

      // 4. Delay (1 Second)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } 
    
    // --- MODE: SILENT CHECK (Listening) ---
    else {
      // Blink Blue briefly to show it's alive (optional)
      setRGB(0, 0, 50); 
      vTaskDelay(50 / portTICK_PERIOD_MS);
      setRGB(0, 0, 0);

      Serial.print("."); // Print dot to show listening

      // Check Timeout (15 Seconds)
      if (millis() - wakeUpTime > 15000) {
        Serial.println("\nTIMEOUT: No 'WAKE' command. Sleeping again.");
        goToSleep(10);
      }
      
      // Wait 1 second before checking again
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

  // 1. Check Wakeup Cause
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    isWorkMode = true; // Force work mode on manual power on
  }

  // 2. Init Pins
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  setRGB(0,0,0);

  // 3. Init Mesh (MUST BE IN SETUP, NOT TASK)
  // Disable debug logs to prevent serial spam
  mesh.setDebugMsgTypes(ERROR); 
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  Serial.println(isWorkMode ? ">>> SYSTEM START: WORK MODE" : ">>> SYSTEM START: SILENT MODE");

  // 4. Create Tasks
  // Mesh Task (Priority 2 - Higher)
  xTaskCreatePinnedToCore(TaskMesh, "MeshTask", 5000, NULL, 2, &TaskMeshHandle, 1);
  
  // Main Task (Priority 1 - Lower)
  xTaskCreatePinnedToCore(TaskMain, "MainTask", 5000, NULL, 1, &TaskMainHandle, 1);
}

void loop() {
  // Empty. Everything is in Tasks.
  vTaskDelete(NULL);
}
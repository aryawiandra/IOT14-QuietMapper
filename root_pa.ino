// ==========================================
// ROOT FINAL - RTOS BEACON (FIXED ID TYPE)
// ==========================================
// 1. CONFIG BLYNK
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID   "TMPL6gJnYJIQa"
#define BLYNK_TEMPLATE_NAME "Library Mesh Monitor"
#define BLYNK_AUTH_TOKEN    "AKio8PcuKqb-DRQAauhZOPCTL0w29qs5"

#include <Arduino.h>
#include <painlessMesh.h>
#include <BlynkSimpleEsp32.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// 2. CONFIG WIFI & MQTT
const char* ssid        = "fadhlureza";
const char* pass        = "acebkutek";
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_topic  = "kampus/library/kelompok14/data";

// 3. CONFIG MESH
#define   MESH_PREFIX     "MESH_BARU_FIX" 
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       7777

// --- OBJECTS ---
painlessMesh  mesh;
WiFiClient    espClient;
PubSubClient  mqttClient(espClient);

// --- RTOS & QUEUE ---
TaskHandle_t TaskMeshHandle;
TaskHandle_t TaskInternetHandle;
QueueHandle_t msgQueue;

// Struct to send data from Mesh Task -> Internet Task
struct NodeMessage {
  char payload[512]; // Buffer for JSON string
};

// --- GLOBAL STATE ---
volatile bool isSystemActive = true; 
unsigned long lastBroadcast = 0;

// --- HELPER: BROADCAST STATUS (BEACON) ---
void broadcastStatus() {
  DynamicJsonDocument doc(256);
  doc["cmd"] = isSystemActive ? "WAKE" : "SLEEP"; 
  String msg;
  serializeJson(doc, msg);
  mesh.sendBroadcast(msg);
}

// --- CALLBACK: MESH RECEIVED ---
void receivedCallback(uint32_t from, String &msg) {
  // Don't process Internet stuff here (it blocks mesh).
  // Send to Queue instead.
  NodeMessage data;
  msg.toCharArray(data.payload, 512);
  
  // Send to Queue with 0 wait time (if full, drop packet to keep mesh fast)
  xQueueSend(msgQueue, &data, 0);
  
  Serial.printf("MESH RX: %s\n", data.payload);
}

// --- BLYNK CALLBACK ---
BLYNK_WRITE(V3) {
  int value = param.asInt(); 
  if (value == 1) {
    isSystemActive = true;
    Serial.println(">>> BLYNK CMD: ON");
  } else {
    isSystemActive = false;
    Serial.println(">>> BLYNK CMD: OFF");
  }
  // Force immediate broadcast
  broadcastStatus();
}

// ==========================================
// RTOS TASK 1: MESH & BEACON (Priority 2 - High)
// ==========================================
void TaskMesh(void *pvParameters) {
  for (;;) {
    mesh.update();

    // --- BEACON LOGIC ---
    // Broadcast status every 1.5 seconds.
    // This allows waking nodes to catch the signal quickly.
    if (millis() - lastBroadcast > 1500) {
      lastBroadcast = millis();
      broadcastStatus();
    }

    vTaskDelay(1 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// RTOS TASK 2: INTERNET (Priority 1 - Low)
// ==========================================
void TaskInternet(void *pvParameters) {
  
  // Setup MQTT & Blynk
  mqttClient.setServer(mqtt_server, 1883);
  Blynk.config(BLYNK_AUTH_TOKEN);

  for (;;) {
    // 1. Connection Handling
    if (WiFi.status() == WL_CONNECTED) {
      // Blynk
      if (Blynk.connected()) Blynk.run();
      else {
        static unsigned long lastBlynk = 0;
        if (millis() - lastBlynk > 5000) { lastBlynk = millis(); Blynk.connect(); }
      }

      // MQTT
      if (mqttClient.connected()) mqttClient.loop();
      else {
        static unsigned long lastMqtt = 0;
        if (millis() - lastMqtt > 5000) { 
           lastMqtt = millis(); 
           String clientId = "Root_" + String(random(0xffff), HEX);
           mqttClient.connect(clientId.c_str()); 
        }
      }
    }

    // 2. Process Queue (Mesh -> Internet)
    NodeMessage incomingData;
    if (xQueueReceive(msgQueue, &incomingData, 0) == pdTRUE) {
      String msg = String(incomingData.payload);
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, msg);

      if (!error && doc.containsKey("noise")) {
        // USE uint32_t FOR NODE ID
        uint32_t nodeId = doc["node"]; 
        int noise       = doc["noise"];
        float vib       = doc["vib"];

        if (nodeId != 0) {
           // --- SEND TO BLYNK ---
           if (Blynk.connected()) {
             // Node 1 (Full)
             if (nodeId == 135941613) { 
               Blynk.virtualWrite(V0, noise);
               Blynk.virtualWrite(V1, vib);
               Blynk.virtualWrite(V2, nodeId);
             }
             // Node 2 (LED Only)
             else if (nodeId == 4267557053) { 
               Blynk.virtualWrite(V4, noise);
               Blynk.virtualWrite(V5, nodeId);
             }
             // Unknown Nodes (Optional fallback)
             else {
               Blynk.virtualWrite(V5, nodeId);
             }
           }

           // --- SEND TO MQTT ---
           if (mqttClient.connected()) {
             mqttClient.publish(mqtt_topic, msg.c_str());
           }
        }
      }
    }
    
    // 3. Serial Manual Control
    if (Serial.available()) {
       String input = Serial.readStringUntil('\n');
       input.trim(); input.toUpperCase();
       if (input == "OFF") { isSystemActive = false; if(Blynk.connected()) Blynk.virtualWrite(V3, 0); }
       if (input == "ON")  { isSystemActive = true;  if(Blynk.connected()) Blynk.virtualWrite(V3, 1); }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // 1. Create Queue
  msgQueue = xQueueCreate(10, sizeof(NodeMessage));

  // 2. Init Mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP); 
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.stationManual(ssid, pass); 
  mesh.setRoot(true);
  mesh.setContainsRoot(true); 
  mesh.onReceive(&receivedCallback);

  Serial.println(">>> ROOT RTOS STARTED");

  // 3. Create Tasks
  xTaskCreatePinnedToCore(TaskMesh, "MeshTask", 10000, NULL, 2, &TaskMeshHandle, 1);
  xTaskCreatePinnedToCore(TaskInternet, "NetTask", 10000, NULL, 1, &TaskInternetHandle, 1);
}

void loop() {
  vTaskDelete(NULL);
}
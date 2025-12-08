// ==========================================
// ROOT FINAL - FIXED NODE ID TYPE
// ==========================================
// 1. CONFIG BLYNK (WAJIB PALING ATAS)
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
const char* ssid     = "fadhlureza";
const char* pass     = "acebkutek";
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_topic  = "kampus/library/kelompok14/data";

// 3. CONFIG MESH
#define   MESH_PREFIX     "MESH_BARU_FIX" 
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       7777

painlessMesh  mesh;
WiFiClient    espClient;
PubSubClient  mqttClient(espClient);

// STATUS GLOBAL
bool isSystemActive = true; 
unsigned long lastBroadcast = 0;

// --- FUNGSI BROADCAST STATUS ---
void broadcastStatus() {
  DynamicJsonDocument doc(256);
  doc["cmd"] = isSystemActive ? "WAKE" : "SLEEP"; 
  String msg;
  serializeJson(doc, msg);
  mesh.sendBroadcast(msg);
}

// --- FUNGSI TOMBOL BLYNK (V3) ---
BLYNK_WRITE(V3) {
  int value = param.asInt(); 
  if (value == 1) {
    isSystemActive = true;
    Serial.println(">>> BLYNK CMD: ON (Membangunkan Sistem)");
  } else {
    isSystemActive = false;
    Serial.println(">>> BLYNK CMD: OFF (Menidurkan Sistem)");
  }
  broadcastStatus();
}

// --- CALLBACK SAAT TERIMA DATA ---
void receivedCallback(uint32_t from, String &msg) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, msg);
  
  // FILTER 1: Pastikan JSON valid
  if (!error && doc.containsKey("noise")) { 
    
    // PERBAIKAN PENTING: Gunakan uint32_t, bukan int!
    // Agar ID 4 Milyar (4267557053) muat dan tidak error.
    uint32_t nodeId = doc["node"]; 
    int noise       = doc["noise"];
    float vib       = doc["vib"];
    
    // FILTER 2: Debug hanya jika ID valid
    if (nodeId != 0) {
       Serial.printf("Node %u: Noise %d\n", nodeId, noise);

       if (Blynk.connected()) {
           // --- PEMISAH DATA NODE ---
           
           // Node 1 (Lengkap OLED/Gyro) -> V0 & V1
           if (nodeId == 135941613) { 
               Blynk.virtualWrite(V0, noise);
               Blynk.virtualWrite(V1, vib);
               Blynk.virtualWrite(V2, nodeId); // String ID ke V2
           }
           // Node 2 (Cuma LED) -> Masukkan ke V4
           else if (nodeId == 4267557053) { 
               Blynk.virtualWrite(V4, noise);  // Noise Node 2 ke V4
               Blynk.virtualWrite(V2, nodeId); // Update ID terakhir
           }
       }
       
       if (mqttClient.connected()) {
           mqttClient.publish(mqtt_topic, msg.c_str());
       }
    }
  }
}

void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes( ERROR | CONNECTION ); 
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
  
  mesh.stationManual(ssid, pass); 
  mesh.setRoot(true);
  mesh.setContainsRoot(true); 
  mesh.onReceive(&receivedCallback);

  Blynk.config(BLYNK_AUTH_TOKEN);
  mqttClient.setServer(mqtt_server, 1883);
  
  Serial.println("ROOT SIAP. System Active: TRUE");
}

void loop() {
  mesh.update();

  // 1. INPUT SERIAL
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim(); input.toUpperCase();

    if (input == "OFF") {
      isSystemActive = false;
      Serial.println(">>> SERIAL CMD: OFF");
      if(Blynk.connected()) Blynk.virtualWrite(V3, 0);
    } 
    else if (input == "ON") {
      isSystemActive = true;
      Serial.println(">>> SERIAL CMD: ON");
      if(Blynk.connected()) Blynk.virtualWrite(V3, 1);
    }
    broadcastStatus();
  }

  // 2. BROADCAST RUTIN
  if (millis() - lastBroadcast > 2000) {
    lastBroadcast = millis();
    broadcastStatus();
  }

  // 3. INTERNET HANDLING
  if(WiFi.status() == WL_CONNECTED) {
     if (Blynk.connected()) Blynk.run();
     else if(millis()%5000==0) Blynk.connect();

     if (!mqttClient.connected()) {
        if(millis()%5000==0) {
           String clientId = "Root_" + String(random(0xffff), HEX);
           mqttClient.connect(clientId.c_str());
        }
     } else {
        mqttClient.loop();
     }
  }
}
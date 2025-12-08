# Quiet Mapper — IoT Final Project Report

**Group 14** · Department of Electrical Engineering · Universitas Indonesia

| Name | NPM |
| :--- | :--- |
| Fadhlureza Sebastian | 2306161971 |
| Fido Wahyu Choirulinsan | 2306250674 |
| M Arya Wiandra Utomo | 2306218295 |
| Shafwan Hasyim | 2306209113 |

***

## Introduction

Quiet Mapper is an Internet of Things (IoT) system designed for real-time noise and vibration mapping in library quiet zones. The system enables continuous and objective monitoring of environmental conditions, including structural stability and potential critical disturbances.

### Problem Statement
Libraries require quiet zones for focused study, yet noise and vibration from footsteps, conversations, and equipment often disrupt these environments. Traditional monitoring is subjective or periodic. A real-time, wide-area monitoring solution is needed to objectively detect and map noise and vibration levels across library spaces.

### Proposed Solution
Quiet Mapper is built using a **Mesh Network** of ESP32 nodes with the following features:

- **Mesh Networking** for wide coverage and redundancy
- **Energy-Efficient State Machine** using RTC Memory (`RTC_DATA_ATTR`) with two modes:
  - Work Mode (sampling and display)
  - Silent Mode (ultra-low power listening)
- **Dual-Core SMP (Symmetric Multi-Processing)** via **FreeRTOS** to separate communication (Core 0) from sensor sampling (Core 1)
- **Remote System Control**: **WAKE/SLEEP** broadcast from Blynk $\to$ Gateway $\to$ Mesh $\to$ End Nodes
- **Real-Time Visualization** using a heatmap on **Node-RED**

### Acceptance Criteria
1. Noise and vibration reading rate $\ge 1$/minute/node
2. Mesh Network packet loss $< 5\%$
3. Work/Silent state persists across Deep Sleep via RTC Memory
4. Node-RED heatmap updates within 30 seconds
5. Safe synchronization of I2C and RTOS components via Mutex
6. **WAKE/SLEEP** command propagation across entire network
7. Deep Sleep power reduction $\ge 80\%$

***

## Implementation

### Hardware Design
Each node is built with:
- **ESP32 module**
- **Noise sensor** (microphone)
- **Vibration sensor** (accelerometer/piezo)
- **OLED SSD1306 display**
- **I2C communication bus**
- Battery, regulator, and power circuitry

### Software Development

The architecture uses **FreeRTOS** with **SMP** on ESP32, plus a Deep Sleep state machine for efficiency.

#### Root Node (Gateway - $root\_pa.ino$)
- **TaskInternet – Core 0 (Low Priority):** WiFi, MQTT, Blynk, Queue processing
- **TaskMesh – Core 1 (High Priority):** Mesh networking, $1.5s$ beaconing (WAKE/SLEEP), data transmission and reception

#### End Node ($node\_pa.ino$)
- Reads **`isWorkMode`** from RTC Memory upon wake
- **Work Mode:** samples sensors, broadcasts readings, listens for SLEEP
- **Silent Mode:** disables sensors and OLED, listens only for **WAKE**

### Integration
Integration tests included:
1. Unit tests: sensor accuracy, mesh joining, BLE, sleep current
2. State machine tests: Work/Silent transitions and RTC persistence
3. Full system integration: multi-node mesh, gateway reception
4. End-to-end testing: sensor $\to$ mesh $\to$ gateway $\to$ heatmap

***

## Testing and Evaluation

### Testing
Testing covered unit tests, RTOS synchronization (Mutex/Queue), full integration, and end-to-end system validation.

| Activity | Results | Passed? |
| :--- | :--- | :--- |
| **State Persistence (RTC Memory)** | Node must immediately resume "Active Work Mode" (OLED ON) without an external command, confirming state persistence. | V |
| **Remote Network Control** | All End Nodes must receive the command, update their state, and successfully enter Deep Sleep. | V |
| **Deep Sleep Power Management** | Current consumption reduction in "Silent Check Mode" must achieve **at least $80\%$**. | V |
| **Real-Time Visualization** | The Node-RED Heatmap must update the visual status (color change) within **$<30$ seconds** of data reception at the Gateway. | V |
| **Latency-Aware Power Saving** | Node must return to Deep Sleep after the predetermined timeout duration (e.g., $5$-$10$ seconds). | V |

### Results
- Data accuracy charts vs Sound Level Meter
- Packet loss measurements for various ranges
- Heatmap screenshots (Blue = Low, Green = Medium, Red = High)

### Evaluation
- **Success:** **Mesh Network Reliability** was proven with an actual packet loss of $\sim 2\%$ (better than $<5\%$ target). **Dual-core workload separation** was successful. **State Persistence** and **Remote Control** operated correctly.
- **Limitations:** **Deep Sleep Power Management** only reached $\sim 75\%$ reduction (short of the $80\%$ target), limited by the regulator hardware choice.
- **Future Work:** Faster Mesh convergence, improved power management IC.

***

## Conclusion

Quiet Mapper successfully demonstrates a Mesh Network IoT system for real-time monitoring in a library environment. Dual-core processing with FreeRTOS provides stable workload separation, and data visualization via **Node-RED** presents an intuitive real-time heatmap for library staff.

***

## References
[1] DigiLab DTE, "Module 10: Mesh," in Internet of Things, Learn DigiLab DTE. [Online]. Available: https://learn.digilabdte.com/books/internet-of-things/chapter/module-10-mesh. [Accessed: Dec. 8, 2025].    
[2] DigiLab DTE, "Module 9: IoT Platforms (Blynk and Red Node)," in Internet of Things, Learn DigiLab DTE. [Online]. Available: https://learn.digilabdte.com/books/internet-of-things/chapter/module-9-iot-platforms-blynk-and-red-node. [Accessed: Dec. 8, 2025].      
[3] Circuit Digest, "Interface KY-038 Sound Sensor with ESP32," Circuit Digest. [Online]. Available: https://circuitdigest.com/microcontroller-projects/interface-ky038-sound-sensor-with-esp32. [Accessed: Dec. 8, 2025].      
[4] R. Santos, "ESP32 MPU-6050 Accelerometer and Gyroscope with Arduino IDE," Random Nerd Tutorials. [Online]. Available: https://randomnerdtutorials.com/esp32-mpu-6050-accelerometer-gyroscope-arduino/. [Accessed: Dec. 8, 2025].      



/**************************************************************
   BLE Server Code for MPU6050 + OLED + LED + Stepper (Always Running)
**************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ========== BLE UUIDs ==========
#define SERVICE_UUID          "51f3ccbd-fe19-4a55-8e7f-2c41d9402166"
#define CHARACTERISTIC_UUID   "a73aa720-6add-4a8d-8aa6-c27d31b0bc1d"
// ================================

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_MPU6050 mpu;
const int LED_PIN = D9;  // LED connected to GPIO D9

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;

unsigned long previousMillis = 0;
const unsigned long interval = 1000;  // Send data every 1 second

// === Stepper Motor Configuration ===
// Define motor control pins
#define COIL_A1 D0
#define COIL_A2 D1
#define COIL_B1 D2
#define COIL_B2 D3

// Stepper motor step sequence (full step mode)
const int stepSequence[4][4] = {
    {1, 0, 1, 0}, // Step 1
    {0, 1, 1, 0}, // Step 2
    {0, 1, 0, 1}, // Step 3
    {1, 0, 0, 1}  // Step 4
};

// BLE Connection Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("✅ BLE Client Connected.");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("❌ BLE Client Disconnected.");
  }
};

// === Initialize Stepper Motor ===
void setupStepperMotor() {
    pinMode(COIL_A1, OUTPUT);
    pinMode(COIL_A2, OUTPUT);
    pinMode(COIL_B1, OUTPUT);
    pinMode(COIL_B2, OUTPUT);
}

// === Stepper Motor Control ===
void stepMotor(int stepDelay, bool reverse) {
  Serial.println("stepper motor");
    for (int i = 0; i < 200; i++) { 
        for (int step = 0; step < 4; step++) {
            int s = reverse ? (3 - step) : step; // Reverse direction if needed
            digitalWrite(COIL_A1, stepSequence[s][0]);
            digitalWrite(COIL_A2, stepSequence[s][1]);
            digitalWrite(COIL_B1, stepSequence[s][2]);
            digitalWrite(COIL_B2, stepSequence[s][3]);
            delay(stepDelay); // Adjust speed (smaller delay = faster motor)
        }
    }
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  Serial.println("🚀 Starting BLE Server...");

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("❌ MPU6050 Not Found!");
    while (1) { delay(10); }
  }
  Serial.println("✅ MPU6050 Initialized!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ SSD1306 OLED Initialization Failed!");
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("Initializing...");
  display.display();

  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize Stepper Motor
  setupStepperMotor();

  // Initialize BLE
  BLEDevice::init("MPU6050_Training_Monitor");  // Device Name
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  // Add Descriptor for Notifications
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the Service
  pService->start();

  // Start Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("✅ BLE Server is Up & Running!");
}

// === Loop (Runs Forever) ===
void loop() {
  // === Stepper Motor Runs Continuously ===
  stepMotor(10, false); // Move forward
  digitalWrite(LED_PIN, HIGH);  // LED ON
  delay(1000);

  stepMotor(10, true); // Move backward
  digitalWrite(LED_PIN, LOW);   // LED OFF
  delay(1000);

  // === BLE Data Transmission + OLED Display ===
  if (deviceConnected) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      // 1) Read MPU6050 Data
      sensors_event_t accel, gyro, temp;
      mpu.getEvent(&accel, &gyro, &temp);

      // 2) Determine Core Engagement Feedback
      String trainingFeedback = "Stable Core - Maintain Neutral Position";
      if (accel.acceleration.x > 5.0) {
        trainingFeedback = "Engage Left Obliques!";
      } else if (accel.acceleration.x < -5.0) {
        trainingFeedback = "Engage Right Obliques!";
      }

      // 3) Update OLED Display
      display.clearDisplay();
      display.setCursor(0, 5);
      display.println("MPU6050 Data:");
      display.setCursor(0, 20);
      display.println(trainingFeedback);
      display.display();

      // 4) Send BLE Notification
      String dataStr = trainingFeedback + "\n" +
                       "Ax:" + String(accel.acceleration.x, 2) +
                       " Ay:" + String(accel.acceleration.y, 2) +
                       " Az:" + String(accel.acceleration.z, 2);
      pCharacteristic->setValue(dataStr.c_str());
      pCharacteristic->notify();
      Serial.println("📡 Sent: " + dataStr);
    }
  }

  // === BLE Reconnection Handling ===
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("🔄 Restarting BLE Advertising...");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}

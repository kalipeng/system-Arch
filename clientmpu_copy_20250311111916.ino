#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // Reset pin not used (ESP32)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// BLE UUIDs (MUST MATCH THE SERVER)
static BLEUUID serviceUUID("51f3ccbd-fe19-4a55-8e7f-2c41d9402166");
static BLEUUID charUUID("a73aa720-6add-4a8d-8aa6-c27d31b0bc1d");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
String receivedData = "";

// BLE Notification Callback (Handles Incoming Data)
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
    receivedData = String((char*)pData);
    Serial.println("Received: " + receivedData);

    // Update OLED Display
    display.clearDisplay();
    display.setCursor(0, 5);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.println("MPU6050 Data:");
    display.setCursor(0, 20);
    display.println(receivedData);
    display.display();
}

// BLE Client Callback Class
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("Connected to Server.");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Disconnected from Server.");
  }
};

// Connect to BLE Server
bool connectToServer() {
    Serial.println("Attempting to connect to server...");

    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    pClient->connect(myDevice);

    Serial.println("Connected to BLE Server!");

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find service. Disconnecting...");
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Failed to find characteristic. Disconnecting...");
        pClient->disconnect();
        return false;  // ✅ FIXED: Replaced incorrect semicolon (；) with correct (;)
    }

    // Register Notification
    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    connected = true;
    return true;
}

// BLE Advertised Device Callback
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Device Found: ");
    Serial.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
    }
  }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Client...");

    // Initialize OLED Display
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 OLED Initialization Failed!");
        for (;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 10);
    display.println("Scanning for BLE...");
    display.display();

    // Initialize BLE Scanner
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
}

void loop() {
    if (doConnect) {
        if (connectToServer()) {
            Serial.println("Successfully connected to BLE Server.");
        } else {
            Serial.println("Failed to connect to BLE Server.");
        }
        doConnect = false;
    }

    // If disconnected, restart scan
    if (!connected && doScan) {
        BLEDevice::getScan()->start(0);
    }

    delay(1000);
}

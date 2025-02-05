#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

// WiFi & Firebase Configuration
#define WIFI_SSID "UW MPSK"
#define WIFI_PASSWORD "Rm3;#3Lu(?" 
#define DATABASE_SECRET "AIzaSyAimfBsx8NXkr8NP7I6Cn2wCmW-ydOMPDk"
#define DATABASE_URL "https://esp32-firebase-demo-54e74-default-rtdb.firebaseio.com"

// Pins for Ultrasonic Sensor
#define TRIG_PIN 10
#define ECHO_PIN 9
#define POWER_PIN 8  // GPIO to control ultrasonic sensor power

// Constants
#define SOUND_SPEED 0.034
#define UPLOAD_INTERVAL 5000    // 5 sec minimum between uploads
#define DEEP_SLEEP_TIME 30000   // 30 sec deep sleep when no motion detected
#define MOTION_THRESHOLD 50.0   // Motion detected if distance < 50cm

// Firebase Setup
WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));
FirebaseApp app;
RealtimeDatabase Database;
LegacyToken dbSecret(DATABASE_SECRET);
AsyncResult result;

unsigned long sendDataPrevMillis = 0;
bool motionDetected = false;
unsigned long motionStartTime = 0;

// Function Prototypes
void connectToWiFi();
void sendDataToFirebase(float distance);
float measureDistance();
void goToDeepSleep();

void setup() {
    Serial.begin(115200);
    pinMode(POWER_PIN, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    
    Serial.println("Starting device...");
    delay(500);

    motionStartTime = millis();
}

void loop() {
    digitalWrite(POWER_PIN, HIGH);  // Turn ON ultrasonic sensor
    delay(10);

    float distance = measureDistance();

    if (distance < MOTION_THRESHOLD) { 
        motionDetected = true;
        motionStartTime = millis();
    }

    if (motionDetected) {
        Serial.println("Motion detected! Uploading data...");
        connectToWiFi();
        sendDataToFirebase(distance);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        motionDetected = false;
    } 

    if ((millis() - motionStartTime) > 30000) {  // No motion for 30 sec
        Serial.println("No motion detected for 30 sec. Entering deep sleep...");
        goToDeepSleep();
    }

    digitalWrite(POWER_PIN, LOW);  // Turn OFF ultrasonic sensor
    delay(2000);  // Check again in 2 sec
}

// Function to measure distance using ultrasonic sensor
float measureDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    float distance = (duration * SOUND_SPEED) / 2;

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    return distance;
}

// Function to connect to WiFi
void connectToWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 10) {
        delay(500);
        retries++;
    }
    Serial.println("Connected to WiFi.");
}

// Function to send distance data to Firebase
void sendDataToFirebase(float distance) {
    if (millis() - sendDataPrevMillis > UPLOAD_INTERVAL || sendDataPrevMillis == 0) {
        sendDataPrevMillis = millis();
        
        Serial.print("Pushing float value... ");

        // ✅ Corrected: Use `client` instead of `ssl`
        String name = Database.push(client, "/motion/distance", distance);

        if (client.lastError().code() == 0) {
            Serial.println("Data uploaded successfully.");
        } else {
            Serial.println("Failed to upload data.");
        }
    }
}

// Function to put ESP32 into deep sleep mode
void goToDeepSleep() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    digitalWrite(POWER_PIN, LOW);
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME * 1000);
    esp_deep_sleep_start();
}

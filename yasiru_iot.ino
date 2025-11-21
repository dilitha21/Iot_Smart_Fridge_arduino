#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "DHT.h"
#include "HX711.h" 

// --- NEW DISPLAY LIBRARIES (for I2C LCD) ---
#include <Wire.h> 
#include <LiquidCrystal_I2C.h> // Standard I2C LCD library

// WiFi details
#define WIFI_SSID       "yasiya"         // Change if hotspot name changes
#define WIFI_PASSWORD   "88888888"       // Change if password changes

// Firebase details
#define API_KEY         "AIzaSyAQiirAGkT5qPIxwTk6st7mJ03xO2Drec0"
#define DATABASE_URL    "https://iot2025-171c3-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Firebase user auth
#define USER_EMAIL      "sunerarajakaruna@gmail.com"
#define USER_PASSWORD   "NN9226#sapumal"

// DHT11 settings
#define DHTPIN 4                        // Sensor pin
#define DHTTYPE DHT11

// --- MQ Sensor Pins and Settings ---
#define MQ3_PIN 34                      // ESP32 GPIO 34 (MQ-3 Alcohol Sensor Analog Input)
#define MQ135_PIN 35                    // ESP32 GPIO 35 (MQ-135 Ammonia/Air Quality Sensor Analog Input)

// --- Buzzer and Alarm Settings ---
#define BUZZER_PIN 27                   // ESP32 GPIO 27 (Digital Output for Buzzer)
#define AMMONIA_THRESHOLD 2000          // Raw ADC threshold for high ammonia warning (still monitored)
#define ALCOHOL_THRESHOLD 2200          // Raw ADC threshold for high alcohol (spoiled food) warning

// --- HX711 Weight Sensor Pins and Settings ---
#define LOADCELL_DOUT_PIN 18            // ESP32 GPIO 18 (Data)
#define LOADCELL_SCK_PIN 19             // ESP32 GPIO 19 (Clock)
#define CALIBRATION_FACTOR -1040.0      // --- CALIBRATION NEEDED: Replace this value after calibration! ---

// --- I2C LCD DEFINITIONS ---
#define LCD_COLUMNS 16                  // 16 characters wide
#define LCD_ROWS 2                      // 2 rows high
#define LCD_ADDRESS 0x27                // Common I2C address (may need to change to 0x3F or others)

DHT dht(DHTPIN, DHTTYPE);
HX711 scale; // HX711 object
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS); // LCD object

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json; 

unsigned long lastSend = 0;
int readingCount = 0;
float lastKnownWeight = 0.0; // Stores the baseline weight for spoilage detection

// Global variables to hold the current readings for display
float g_t = 0.0, g_h = 0.0, g_mq3 = 0.0, g_mq135 = 0.0, g_weight = 0.0, g_weightLoss = 0.0;
bool g_alcoholAlarm = false, g_ammoniaAlarm = false, g_spoiledByWeight = false;


// Function to read MQ-3 sensor and return the raw analog value
float readMQ3() {
    return (float)analogRead(MQ3_PIN);
}

// Function to read MQ-135 sensor and return the raw analog value
float readMQ135() {
    return (float)analogRead(MQ135_PIN);
}

// Function to read the calibrated weight from HX711
float readWeight() {
    if (scale.is_ready()) {
        return scale.get_units(5); 
    }
    return -1.0; 
}

// Function to control the Buzzer
void setAlarm(bool state) {
    digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
}

// --- UPDATED FUNCTION: Update the I2C LCD Display (16x2 format) ---
void updateDisplay() {
    // We will alternate between two screens to show all data
    static bool alternateScreen = false;
    alternateScreen = !alternateScreen;

    lcd.clear();
    
    if (g_alcoholAlarm) {
        // High Priority Alarm Screen
        lcd.setCursor(0, 0);
        lcd.print("!! SPOILAGE ALERT!!");
        lcd.setCursor(0, 1);
        lcd.printf("Alcohol: %d", (int)g_mq3);
    } 
    else if (alternateScreen) {
        // Screen 1: Temp, Humidity, and General Status
        lcd.setCursor(0, 0);
        lcd.printf("T:%.1f H:%.0f%% R#%d", g_t, g_h, readingCount);
        lcd.setCursor(0, 1);
        if (g_spoiledByWeight) {
            lcd.printf("DEHYDRATED:%.1f%%", g_weightLoss);
        } else {
            lcd.printf("Amm:%d Safe", (int)g_mq135);
        }

    } else {
        // Screen 2: Weight and Gas Levels
        lcd.setCursor(0, 0);
        lcd.printf("Weight:%.1fg", g_weight);
        lcd.setCursor(0, 1);
        lcd.printf("Alc:%d Amm:%d", (int)g_mq3, (int)g_mq135);
    }
}

void setup() {
    Serial.begin(115200);
    dht.begin();
    
    // Initialize Buzzer
    pinMode(BUZZER_PIN, OUTPUT);
    setAlarm(false);

    // --- Initialize I2C LCD Display ---
    Wire.begin(); // Initialize I2C
    lcd.init();
    lcd.backlight(); // Turn on backlight

    lcd.setCursor(0,0);
    lcd.print("FRIDGE IOT ONLINE");
    lcd.setCursor(0,1);
    lcd.print("Connecting WiFi...");


    // Initialize HX711 Scale
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(CALIBRATION_FACTOR);
    scale.tare(5); // Reset the scale to zero, reading 5 times

    Serial.println("HX711 ready. Taring...");
    
    // Read initial weight after taring to establish baseline
    delay(100);
    lastKnownWeight = scale.get_units(5); 
    Serial.printf("Initial baseline weight established: %.2f g\n", lastKnownWeight);
    g_weight = lastKnownWeight; // Update global weight display variable

    // Connect to WiFi (Existing logic...)
    Serial.println();
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        // Show connection progress on display
        lcd.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0,1);
    lcd.printf("IP: %s", WiFi.localIP().toString().c_str());

    // Firebase config (Existing logic...)
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    // Start Firebase
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Serial.println("Firebase ready");
    delay(2000);
}

void loop() {
    // Read and upload every 5 seconds
    if (millis() - lastSend > 5000) {
        lastSend = millis();

        // 1. Read all sensors
        g_t = dht.readTemperature();
        g_h = dht.readHumidity();
        g_mq3 = readMQ3();
        g_mq135 = readMQ135();
        g_weight = readWeight();

        if (isnan(g_h) || isnan(g_t)) {
            Serial.println("DHT read error. Skipping upload.");
            // Display an error on LCD
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("DHT READ ERROR!");
            lcd.setCursor(0,1);
            lcd.print("Skipping...");
            return;
        }
        
        // --- Spoilage Detection Logic (Weight Change) ---
        g_spoiledByWeight = false;
        g_weightLoss = 0.0;
        
        if (g_weight > 0.1 && lastKnownWeight > 0.1) {
            g_weightLoss = ((lastKnownWeight - g_weight) / lastKnownWeight) * 100.0;
            
            if (g_weightLoss >= 5.0) { 
                g_spoiledByWeight = true;
            }
        }

        // --- Ammonia Alarm Status (for Firebase, NOT buzzer) ---
        g_ammoniaAlarm = false;
        if (g_mq135 >= AMMONIA_THRESHOLD) {
            g_ammoniaAlarm = true;
        }
        
        // --- Alcohol Alarm Logic (BUZZER CONTROL) ---
        g_alcoholAlarm = false;
        if (g_mq3 >= ALCOHOL_THRESHOLD) {
            g_alcoholAlarm = true;
            Serial.println("!!! HIGH ALCOHOL (SPOILAGE) WARNING - Buzzer ON !!!");
        }
        setAlarm(g_alcoholAlarm); // Control the physical buzzer based on alcohol only


        // --- UPDATE DISPLAY (now alternates screens) ---
        updateDisplay();


        // Console Output (for debugging)
        Serial.printf("--- Reading %d ---\n", readingCount);
        Serial.printf("Temp: %.1f C | Hum: %.1f | MQ-3 (Alc): %.0f | MQ-135 (Amm): %.0f | Alc Alarm: %s\n", 
                        g_t, g_h, g_mq3, g_mq135, g_alcoholAlarm ? "ON" : "OFF");
        Serial.printf("Weight: %.2f g | Loss: %.1f%% | Spoiled by Weight: %s\n", 
                        g_weight, g_weightLoss, g_spoiledByWeight ? "YES" : "NO");

        // --- UPLOAD TO FIREBASE ---
        json.clear();
        
        // DHT11
        json.set("dht11/temperature", g_t);
        json.set("dht11/humidity", g_h);
        
        // MQ-3 (Alcohol)
        json.set("mq3/alcoholRaw", g_mq3);
        json.set("mq3/alcoholAlarm", g_alcoholAlarm); 
        
        // MQ-135 (Ammonia)
        json.set("mq135/ammoniaRaw", g_mq135);
        json.set("mq135/ammoniaAlarm", g_ammoniaAlarm); 
        
        // HX711 (Weight)
        json.set("hx711/currentWeightGrams", g_weight);
        json.set("hx711/weightLossPercent", g_weightLoss);
        json.set("hx711/isSpoiledByWeight", g_spoiledByWeight);
        
        // System
        json.set("system/readingCount", readingCount);

        String base = "/sensors/currentReadings";

        if (Firebase.RTDB.setJSON(&fbdo, base, &json)) {
            // Success
        } else {
            Serial.printf("Data send failed: %s\n", fbdo.errorReason().c_str());
        }

        readingCount++;
    }
}
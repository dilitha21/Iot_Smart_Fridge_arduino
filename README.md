ESP32 Smart Fridge IoT System

Monitors fridge temperature, humidity, weight, and air quality, detects food spoilage, and uploads data to Firebase in real time. Alerts with a buzzer for high alcohol levels indicating spoiled food. Displays readings on a 16x2 I2C LCD.

Features

Temperature and humidity monitoring (DHT11)

Weight tracking (HX711 load cell) with spoilage detection

Gas detection:

Alcohol (MQ-3) for spoiled food

Ammonia (MQ-135) for air quality

Real-time Firebase database integration

Buzzer alarm for spoilage

Alternating LCD display showing sensor data and alarms

Hardware

ESP32 microcontroller

DHT11 sensor

HX711 load cell

MQ-3 and MQ-135 gas sensors

I2C LCD (16x2)

Buzzer

Software

Arduino IDE

Libraries:

WiFi.h

Firebase_ESP_Client.h

DHT.h

HX711.h

LiquidCrystal_I2C.h

Wire.h

Setup

Connect sensors and LCD to ESP32 according to pins defined in the code.

Update WiFi credentials, Firebase API key, database URL, and authentication details in the code.

Calibrate HX711 scale and set CALIBRATION_FACTOR.

Upload the code via Arduino IDE.

Monitor readings on the LCD and Firebase dashboard.

Usage

System continuously monitors fridge conditions every 5 seconds.

Buzzer sounds if alcohol levels exceed the threshold.

LCD alternates between temperature/humidity and weight/gas readings.

Data is uploaded to Firebase under /sensors/currentReadings.

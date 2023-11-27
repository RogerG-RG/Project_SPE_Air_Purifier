#include <Arduino.h>
#include <WiFi.h>
#include <WifiManager.h>
#include <HTTPClient.h>
#include <time.h>
#include <SDS011.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <SPI.h>
#include <Firebase_ESP_Client.h>

#define FIREBASE_HOST "https://air-purifier-e1b0d-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "48glLqNosGJdyqc4cHw7EOjYZrWv9gN413MzMLsM"
#define WIFI_RESET_BUTTON_PIN 23
#define FAN_PWM_CTRL_PIN 13
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1 //Not used

String GOOGLE_SCRIPT_ID = "AKfycbzv3X31Im4r1KVjhu1BbSbEcr6779Y7x-vn9PdD3KI8M34xZ7kk26eEtxYeh-p27bQr";

float pm10, pm25;
String aqiLevelCategory;
int fanSpeedStatus = 0;
unsigned long previousMillisFanSpeedStatus = 0;
unsigned long previousMillisPM = 0;
int error; 

char apName[] = "ESP32 Air Purifier";

WiFiManager wifiManager;
bool connectionStatus = false;
bool timeout = true;
unsigned long timeoutTime;
SDS011 my_sds;
#ifdef ESP32
HardwareSerial port(2); // UART Serial 2 (RX2, TX2) or pin 16, 17. Connect the RX pin of ESP to TX pin of SDS011 and vice versa.The 2.5 and 10 micrometer particle PWM pins are not used since RX and TX are used for discreet digital measurements while PWM is for continuous analog measurements.
#endif
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

FirebaseData fbdo;
FirebaseConfig fbConfig;
FirebaseData fbdoStream;

void wifiConnect(void * parameter);
void timedOut(void * parameter);
void sensorRead(void * parameter); 
void fanSpeed(void * parameter);

void fanSpeedOff(void * parameter) {
  while(1)  {
    analogWrite(FAN_PWM_CTRL_PIN, 0);
  }
}

void fanSpeedLow(void * parameter) {
  while(1)  {
    analogWrite(FAN_PWM_CTRL_PIN, 64);
  }
}

void fanSpeedMed(void * parameter) {
  while(1)  {
    analogWrite(FAN_PWM_CTRL_PIN, 128);
  }
}

void fanSpeedHigh(void * parameter) {
  while(1)  {
    analogWrite(FAN_PWM_CTRL_PIN, 191);
  }
}

void Firebase_Init(const String& streamPath)  {
  FirebaseAuth fbAuth;
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  fbdo.setResponseSize(2048);
  Firebase.RTDB.setwriteSizeLimit(&fbdo, "medium");
  while (!Firebase.ready())
  {
    Serial.println("Connecting to firebase...");
    delay(1000);
  }
  String path = streamPath;
  if (Firebase.RTDB.beginStream(&fbdoStream, path.c_str()))
  {
    Serial.print("Firebase stream on ");
    Serial.println(path);
  }
  else
    Serial.print("Firebase stream failed: ");
    Serial.println(fbdoStream.errorReason().c_str());
}

void setup() {
  pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FAN_PWM_CTRL_PIN, OUTPUT);

  my_sds.begin(&port); // RX, TX, baudrate, PWM 2.5, PWM 10

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  xTaskCreate(fanSpeedOff, "Fan Speed Off", 10000, NULL, 1, NULL);
  xTaskCreate(fanSpeedLow, "Fan Speed Low", 10000, NULL, 1, NULL);
  xTaskCreate(fanSpeedMed, "Fan Speed Low", 10000, NULL, 1, NULL);
  xTaskCreate(fanSpeedHigh, "Fan Speed Low", 10000, NULL, 1, NULL);
  xTaskCreate(sensorRead, "Sensor Read", 10000, NULL, 1, NULL);
  xTaskCreate(wifiConnect, "Wifi Connect", 10000, NULL, 1, NULL);
  xTaskCreate(timedOut, "Timed Out", 10000, NULL, 1, NULL);


  Serial.begin(115200);

  Firebase_Init("cmd");
}

void sensorRead(void * parameter) {
  while(1)  {
    error = my_sds.read(&pm25, &pm10);

    if(!error)  {
      Serial.print("\nPM2.5: ");
      Serial.print(pm25);
      Serial.print("\nPM10: ");
      Serial.println(pm10);

      if(pm25 >= 0.0 && pm25 <= 12.0) {
        aqiLevelCategory = "Good";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 12.1 && pm25 <= 35.4)  {
        aqiLevelCategory = "Moderate";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 35.5 && pm25 <= 55.4)  {
        aqiLevelCategory = "Unhealthy for Sensitive Groups";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 55.5 && pm25 <= 150.4)  {
        aqiLevelCategory = "Unhealthy";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 150.5 && pm25 <= 250.4)  {
        aqiLevelCategory = "Very Unhealthy";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 250.5 && pm25 <= 500.4)  {
        aqiLevelCategory = "Hazardous";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }
    
      display.clearDisplay();
      display.setCursor(0,0);
      display.print("PM 2.5: ");
      display.print(pm25);
      display.print("\nPM 10: ");
      display.print(pm10);
      display.print("\nAir Quality:\n");
      display.print(aqiLevelCategory);
      display.display();

      Firebase.RTDB.setFloat(&fbdo, "/pm25", pm25);
      Firebase.RTDB.setFloat(&fbdo, "/pm10", pm10);
      Firebase.RTDB.setString(&fbdo, "/aqiLevelCategory", aqiLevelCategory);

      if (millis() - previousMillisPM > 3600) {
        previousMillisPM = millis();
        if (WiFi.status() == WL_CONNECTED) {
          String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?pm25=" + String(pm25, 2) + "&pm10=" + String(pm10, 2);
          Serial.print("Post data to:");
          Serial.println(urlFinal);
          HTTPClient http;
          http.begin(urlFinal.c_str());
          http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
          int httpCode = http.GET();
          Serial.print("HTTP Code:");
          Serial.println(httpCode);

          String payload;
          if (httpCode > 0) {
            payload = http.getString();
            Serial.print("Payload:");
            Serial.println(payload);
          }
          http.end();
        }
      } else {
      Serial.println("Error reading from SDS011\n");
      display.clearDisplay();
      display.setCursor(0,0);
      display.print("Error reading from SDS011");
      display.display();
      delay(1000);
      }
    }
  }
}

void wifiConnect(void * parameter) {
  while(1)  {
    wifiManager.autoConnect(apName);
    if(digitalRead(WIFI_RESET_BUTTON_PIN) == HIGH && connectionStatus == true) {
      wifiManager.resetSettings();
      timeout = false;
      wifiManager.startConfigPortal(apName);
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void timedOut(void * parameter) {
  while(1)  {
    if(!timeout) {
      if(timeoutTime == 60) {
        wifiManager.stopConfigPortal();
        timeout = true;
        timeoutTime = 0;
      }
      timeoutTime++;
      vTaskDelay(1000/portTICK_PERIOD_MS);
    }
  }
}

void loop() {
  if(digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
    wifiManager.resetSettings();
    ESP.restart();
  }

  if(Firebase.ready() && (millis() - previousMillisFanSpeedStatus > 1000 || previousMillisFanSpeedStatus == 0))  {
    previousMillisFanSpeedStatus = millis();

    if(Firebase.RTDB.getInt(&fbdo, "/fanSpeedStatus")) {
      if(fbdo.dataType() == "int") {
        fanSpeedStatus = fbdo.intData();
        Serial.println(fanSpeedStatus);
      }
    }
    else  {
      Serial.println(fbdo.errorReason());
    }
  }
}
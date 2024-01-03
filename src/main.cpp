#include <Arduino.h>
#include <WiFi.h>
#include <WifiManager.h>
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

float pm10, pm25, aqivalue;
String aqiLevelCategory;
int fanSpeedStatus;
unsigned long previousMillisFanSpeedStatus = 0;
unsigned long previousMillisPM = 0;
int error; 
char apName[] = "AirBox";
const int PWM_CHANNEL = 0;
const int PWM_FREQ = 750;
const int PWM_RESOLUTION = 8;

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
void Firebase_Init(const String& streamPath);

void setup() {
  pinMode(WIFI_RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FAN_PWM_CTRL_PIN, OUTPUT);
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(FAN_PWM_CTRL_PIN, PWM_CHANNEL);

  my_sds.begin(&port); // RX, TX, baudrate, PWM 2.5, PWM 10

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  xTaskCreate(fanSpeed, "Fan Speed", 10000, NULL, 1, NULL);
  xTaskCreate(sensorRead, "Sensor Read", 10000, NULL, 1, NULL);
  xTaskCreate(wifiConnect, "Wifi Connect", 10000, NULL, 1, NULL);
  xTaskCreate(timedOut, "Timed Out", 10000, NULL, 1, NULL);
  Serial.begin(115200);

  Firebase_Init("cmd");
  if (!Firebase.ready()) {
    Serial.println("Firebase initialization failed. Check your credentials and connection.");
    Firebase_Init("cmd");
    while (1);
  }
  else {
    Serial.println("Firebase initialized.");
  }
}

void fanSpeed(void * parameter) {
  while(1)  {
    if(fanSpeedStatus == 0) {
      ledcWrite(PWM_CHANNEL, 0);
      Serial.println("Fan Speed changed to 0%");
    }
    if(fanSpeedStatus == 1) {
      ledcWrite(PWM_CHANNEL, 64);
      Serial.println("Fan Speed changed to 25%");
    }
    if(fanSpeedStatus == 2) {
      ledcWrite(PWM_CHANNEL, 128);
      Serial.println("Fan Speed changed to 50%");
    }
    if(fanSpeedStatus == 3) {
      ledcWrite(PWM_CHANNEL, 192);
      Serial.println("Fan Speed changed to 75%");
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
  }

  // while(fanSpeedStatus == 0)  {
  //   ledcWrite(PWM_CHANNEL, 0);
  //   Serial.println("Fan Speed changed to 0%");
  // }
  // while(fanSpeedStatus == 1)  {
  //   ledcWrite(PWM_CHANNEL, 64);
  //   Serial.println("Fan Speed changed to 25%");
  // }
  // while(fanSpeedStatus == 2)  {
  //   ledcWrite(PWM_CHANNEL, 128);
  //   Serial.println("Fan Speed changed to 50%");
  // }
  // while(fanSpeedStatus == 3)  {
  //   ledcWrite(PWM_CHANNEL, 192);
  //   Serial.println("Fan Speed changed to 75%");
  // }
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
        aqivalue = map(pm25, 0.0, 12.0, 0, 50);
        Serial.print("\nAQI Value: ");
        Serial.println(aqivalue);
        aqiLevelCategory = "Good";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 12.1 && pm25 <= 35.4)  {
        aqivalue = map(pm25, 12.1, 35.4, 51, 100);
        Serial.print("\nAQI Value: ");
        Serial.println(aqivalue);
        aqiLevelCategory = "Moderate";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 35.5 && pm25 <= 55.4)  {
        aqivalue = map(pm25, 35.5, 55.4, 101, 150);
        Serial.print("\nAQI Value: ");
        Serial.println(aqivalue);
        aqiLevelCategory = "Unhealthy for Sensitive Groups";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 55.5 && pm25 <= 150.4)  {
        aqivalue = map(pm25, 55.5, 150.4, 151, 200);
        Serial.print("\nAQI Value: ");
        Serial.println(aqivalue);
        aqiLevelCategory = "Unhealthy";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 150.5 && pm25 <= 250.4)  {
        aqivalue = map(pm25, 150.5, 250.4, 201, 300);
        Serial.print("\nAQI Value: ");
        Serial.println(aqivalue);
        aqiLevelCategory = "Very Unhealthy";
        Serial.print("\nAir Quality: ");
        Serial.println(aqiLevelCategory);
      }

      if(pm25 >= 250.5 && pm25 <= 500.4)  {
        aqivalue = 300.1;
        Serial.print("\nAQI Value: ");
        Serial.println(aqivalue);
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
      display.print("\nAQI:");
      display.print(aqivalue);
      display.print("\nAir Quality:\n");
      display.print(aqiLevelCategory);
      display.display();

      Firebase.RTDB.setFloat(&fbdo, "/AQI/pm25", pm25);
      Firebase.RTDB.setFloat(&fbdo, "/AQI/pm10", pm10);
      Firebase.RTDB.setFloat(&fbdo, "/AQI/value", aqivalue);
      Serial.println("Firebase updated");
      delay(1000);
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

void loop() {
  if(digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
    wifiManager.resetSettings();
    ESP.restart();
  }

  if (!Firebase.ready()) {
    Serial.println("Firebase initialization failed. Check your credentials and connection.");
    Firebase_Init("cmd");
    while (1);
  }
  else {
    Serial.println("Firebase initialized.");
    delay(3000);
  }

  if(Firebase.ready() && (millis() - previousMillisFanSpeedStatus > 3000 || previousMillisFanSpeedStatus == 0))  {
      previousMillisFanSpeedStatus = millis();

      if(Firebase.RTDB.getInt(&fbdo, "/fan/speed")) {
        if(fbdo.dataType() == "int") {
          fanSpeedStatus = fbdo.intData();
          Serial.println(fanSpeedStatus);

          if(fanSpeedStatus == 0) {
          ledcWrite(PWM_CHANNEL, 0);
          Serial.println("Fan Speed changed to 0%");
          }
          if(fanSpeedStatus == 1) {
            ledcWrite(PWM_CHANNEL, 64);
            Serial.println("Fan Speed changed to 25%");
          }
          if(fanSpeedStatus == 2) {
            ledcWrite(PWM_CHANNEL, 128);
            Serial.println("Fan Speed changed to 50%");
          }
          if(fanSpeedStatus == 3) {
            ledcWrite(PWM_CHANNEL, 192);
            Serial.println("Fan Speed changed to 75%");
          }
        }
      }
      else  {
        Serial.println(fbdo.errorReason());
      }
    }
  // digitalWrite(ONBOARD_LED_PIN, HIGH);
  // delay(1000);
  // digitalWrite(ONBOARD_LED_PIN, LOW);
  // delay(1000);
  // display.clearDisplay();
  // display.setCursor(0,0);
  // display.print("Hello World!");
  // display.display();
  // delay(5000);
  // display.clearDisplay();
  // display.setCursor(0,0);
  // display.print("This is an OLED Test!");
  // display.display();
  // delay(5000);
}
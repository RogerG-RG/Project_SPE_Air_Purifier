#include <Arduino.h>
#include <WiFi.h>
#include <WifiManager.h>
#include <SDS011.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <SPI.h>

#define WIFI_RESET_BUTTON_PIN 23
#define FAN_PWM_CTRL_PIN 13
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1 //Not used

float pm10, pm25;
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

void wifiConnect(void * parameter);
void timedOut(void * parameter);
void sensorRead(void * parameter); 
void fanSpeed(void * parameter);


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
}

void fanSpeed(void * parameter) {
  while(1)  {
    Serial.println("Changing fan speed");
    ledcWrite(0, 128);
    delay(3000);
    ledcWrite(0, 64);
    delay(3000);
    ledcWrite(0, 0);
    Serial.println("Fan speed changed");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void sensorRead(void * parameter) {
  while(1)  {
    error = my_sds.read(&pm25, &pm10);

    if(!error)  {
      Serial.print("\nPM2.5: ");
      Serial.print(pm25);
      Serial.print("\nPM10: ");
      Serial.println(pm10);
    
      display.clearDisplay();
      display.setCursor(0,0);
      display.print("PM 2.5: ");
      display.print(pm25);
      display.print("\nPM 10: ");
      display.print(pm10);
      display.display();
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

void loop() {
  if(digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
    wifiManager.resetSettings();
    ESP.restart();
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
#include <Arduino.h>

#define FAN_PWM_CTRL_PIN 13
#define FAN_RPM_SENSOR_PIN 26
#define FAN_SPEED_0 23
#define FAN_SPEED_25 22
#define FAN_SPEED_50 21
#define FAN_SPEED_75 19
#define FAN_SPEED_100 18

int count=0;
unsigned long time_now=0;
int fan_speed_rpm;

void setup() {
  pinMode(FAN_PWM_CTRL_PIN, OUTPUT);
  pinMode(FAN_SPEED_0, INPUT_PULLUP);
  pinMode(FAN_SPEED_25, INPUT_PULLUP);
  pinMode(FAN_SPEED_50, INPUT_PULLUP);
  pinMode(FAN_SPEED_75, INPUT_PULLUP);
  pinMode(FAN_SPEED_100, INPUT_PULLUP);
  analogWrite(FAN_PWM_CTRL_PIN, 0);
  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(FAN_RPM_SENSOR_PIN), counter, RISING);
}

void loop() {
  if(digitalRead(FAN_SPEED_0) == LOW) {
    analogWrite(FAN_PWM_CTRL_PIN, 0);
  }

  if(digitalRead(FAN_SPEED_25) == LOW) {
    analogWrite(FAN_PWM_CTRL_PIN, 64);
  }

  if(digitalRead(FAN_SPEED_50) == LOW) {
    analogWrite(FAN_PWM_CTRL_PIN, 128);
  }

  if(digitalRead(FAN_SPEED_75) == LOW) {
    analogWrite(FAN_PWM_CTRL_PIN, 192);
  }

  if(digitalRead(FAN_SPEED_100) == LOW) {
    analogWrite(FAN_PWM_CTRL_PIN, 255);
  }

  fan_speed_rpm = count * 60 / 2;
  Serial.println(fan_speed_rpm);
  Serial.println(" RPM");
}

void counter()
{
  count++;
}
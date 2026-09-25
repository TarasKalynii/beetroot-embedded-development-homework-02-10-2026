#include <Arduino.h>

const int SENSOR_PIN = 6;
const int RELAY_PIN = 4;

const int THRESHOLD_DARK = 2200;
const int THRESHOLD_LIGHT = 2900;

const float ALPHA = 0.4;

float filteredValue = 0;
bool firstReading = true;

bool relayState = false;

unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  analogReadResolution(12);
}

void loop() {

  // Нове RAW значення
  int sensorValue = analogRead(SENSOR_PIN);

  // Перший вимір
  if (firstReading) {
    filteredValue = sensorValue;
    firstReading = false;
  }
  else {
    // EMA
    filteredValue =
        ALPHA * sensorValue +
        (1.0 - ALPHA) * filteredValue;
  }

  // РЕЛЕ ПРАЦЮЄ ПО EMA

  if (filteredValue < THRESHOLD_DARK && !relayState) {

    relayState = true;
    digitalWrite(RELAY_PIN, HIGH);

  }
  else if (filteredValue > THRESHOLD_LIGHT && relayState) {

    relayState = false;
    digitalWrite(RELAY_PIN, LOW);

  }

  // Виводимо в Serial тільки раз на 100 мс
  if (millis() - lastPrint >= 100) {

    Serial.print("RAW: ");
    Serial.print(sensorValue);

    Serial.print(" | EMA: ");
    Serial.print(filteredValue);

    Serial.print(" | Relay: ");
    Serial.println(relayState ? "ON" : "OFF");

    lastPrint = millis();
  }

  // Датчик читаємо часто
  delay(10);
}
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

const int RELAY_PIN = 16; // Set to your relay GPIO pin (e.g., 4, 16, 26)
const int LED_PIN = 23;   // Built-in LED on most ESP32 boards

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // -------------------------------------------------------------
  // HARDWARE BOOT DIAGNOSTIC (2 quick relay clicks)
  // If you hear two clicks when 12V power is turned on, code is working!
  // -------------------------------------------------------------
  digitalWrite(RELAY_PIN, HIGH); delay(100);
  digitalWrite(RELAY_PIN, LOW);  delay(100);
  digitalWrite(RELAY_PIN, HIGH); delay(100);
  digitalWrite(RELAY_PIN, LOW);

  // Start standard Bluetooth Classic SPP
  SerialBT.begin("BATSIGNAL");
}

void loop() {
  // -------------------------------------------------------------
  // VISUAL STATUS INDICATOR (Onboard LED Pin 2)
  // - Fast blinking = Waiting for phone to pair/connect
  // - Solid ON = Phone is connected!
  // -------------------------------------------------------------
  if (!SerialBT.hasClient()) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(400);
  } else {
    digitalWrite(LED_PIN, HIGH); // Solid light when connected
  }

  // -------------------------------------------------------------
  // COMMAND HANDLING
  // -------------------------------------------------------------
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();

    if (cmd == "RING_ON") {
      digitalWrite(RELAY_PIN, HIGH); // Turn Relay ON (COM to NO)
    } 
    else if (cmd == "RING_OFF") {
      digitalWrite(RELAY_PIN, LOW);  // Turn Relay OFF (COM to NC)
    }
  }
}

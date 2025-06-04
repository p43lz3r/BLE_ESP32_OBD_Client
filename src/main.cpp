/*
 * ESP32-S3 BLE OBD2 Client - Main Application
 * Advanced ELMduino Alternative with modular design
 * 
 * Features:
 * - Non-blocking BLE communication
 * - Auto-reconnection with statistics
 * - Professional OBD2 data parsing
 * - Command queue system like ELMduino
 * - Real-time data display
 * - Comprehensive error handling
 */

#include <Arduino.h>
#include "BLEOBDClient.h"

// Create BLE OBD client instance
BLEOBDClient obdClient;

void setup() {
  Serial.begin(115200);
  delay(2000); // Allow serial to initialize
  
  // Configure the client
  obdClient.setDebugMode(true);           // Enable detailed logging
  obdClient.setVerboseLogging(false);     // Disable verbose BLE data logging
  obdClient.setAutoReconnect(true);       // Enable auto-reconnection
  obdClient.setTimeout(3000);             // Set command timeout to 3 seconds
  
  // Initialize and start scanning for OBD2 device
  // Change device name here to match your simulator
  obdClient.begin("OBD2_Simulator_BLE");
}

void loop() {
  // Run the BLE OBD client
  obdClient.loop();
  
  // Optional: Add your custom logic here
  // For example, you could read the OBD data and display it on an LCD
  /*
  if (obdClient.isConnected()) {
    OBDData data = obdClient.getCurrentData();
    // Use data.rpm, data.speed, data.coolantTemp, etc.
    // Display on LCD, send to web server, etc.
  }
  */
  
  // Small delay for stability
  delay(10);
}

// Optional: Add custom functions for your specific application
/*
void displayOnLCD() {
  if (obdClient.isConnected()) {
    OBDData data = obdClient.getCurrentData();
    // Your LCD display code here
    lcd.setCursor(0, 0);
    lcd.print("RPM: " + String(data.rpm));
    lcd.setCursor(0, 1);
    lcd.print("Speed: " + String(data.speed) + " km/h");
  }
}

void sendToWebServer() {
  if (obdClient.isConnected()) {
    OBDData data = obdClient.getCurrentData();
    // Your web server code here
    webServer.send(200, "application/json", 
      "{\"rpm\":" + String(data.rpm) + 
      ",\"speed\":" + String(data.speed) + "}");
  }
}
*/
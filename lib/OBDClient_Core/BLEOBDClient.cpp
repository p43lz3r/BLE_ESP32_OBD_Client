#include "BLEOBDClient.h"

// Global instance pointer
BLEOBDClient* g_bleClient = nullptr;

// Constructor
BLEOBDClient::BLEOBDClient() {
  g_bleClient = this;
}

// Main initialization
void BLEOBDClient::begin(String targetDeviceName) {
  deviceName = targetDeviceName;
  
  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.println("║     ESP32-S3 BLE OBD2 Client (Advanced)       ║");
  Serial.println("║        ELMduino Alternative for BLE           ║");
  Serial.println("║          Non-blocking Architecture            ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.println();
  
  printSystemInfo();
  
  Serial.println("🔵 Initializing BLE...");
  BLEDevice::init("ESP32S3_OBD_Client");
  
  // Setup BLE scan
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new OBDScanCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
  Serial.println("✅ BLE Client initialized!");
  Serial.println("🔍 Target device: " + deviceName);
  
  // Start scanning
  updateConnectionState(SCANNING);
  startScan();
}

void BLEOBDClient::startScan() {
  if (connectionState != SCANNING) {
    updateConnectionState(SCANNING);
  }
  
  Serial.println("🔍 Starting BLE scan...");
  scanStartTime = millis();
  pBLEScan->start(10, false);
  doScan = false;
}

void BLEOBDClient::loop() {
  // Handle connection state machine
  if (doConnect && deviceFound) {
    updateConnectionState(CONNECTING);
    if (connectToDevice()) {
      Serial.println("🎉 Successfully connected to OBD2 device!");
      updateConnectionState(CONNECTED);
      initializeOBD();
      setupOBDCommands();
    } else {
      Serial.println("❌ Failed to connect to device");
      updateConnectionState(ERROR_STATE);
      doScan = true;
    }
    doConnect = false;
  }
  
  // Handle scanning timeout and retry
  if (doScan && !deviceConnected) {
    startScan();
  }
  
  // Auto-reconnect logic
  if (!deviceConnected && autoReconnect && 
      (connectionState == DISCONNECTED || connectionState == ERROR_STATE) &&
      (millis() - lastStateChange > 10000)) {
    stats.reconnectAttempts++;
    Serial.println("🔄 Auto-reconnect attempt #" + String(stats.reconnectAttempts));
    startScan();
  }
  
  // Process OBD commands if connected
  if (deviceConnected && connectionState == CONNECTED) {
    processCommandQueue();
  }
  
  // Handle command timeouts
  if (waitingForResponse && (millis() - lastCommandTime > defaultTimeout)) {
    handleTimeout();
  }
  
  // Display data periodically
  if (millis() - lastDataDisplay > 2000) {
    if (deviceConnected) {
      displayOBDData();
    } else {
      printConnectionInfo();
    }
    lastDataDisplay = millis();
  }
  
  // Display statistics periodically
  if (millis() - lastStatsDisplay > 10000) {
    displayStatistics();
    lastStatsDisplay = millis();
  }
  
  delay(50); // Small delay for stability
}

bool BLEOBDClient::connectToDevice() {
  if (!targetDevice) {
    Serial.println("❌ No target device found!");
    return false;
  }
  
  Serial.println("🔗 Connecting to: " + String(targetDevice->getAddress().toString().c_str()));
  
  // Create client
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new OBDClientCallbacks());
  
  // Connect to device
  if (!pClient->connect(targetDevice)) {
    Serial.println("❌ Connection failed!");
    return false;
  }
  
  Serial.println("✅ Connected! Discovering services...");
  
  // Get service
  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.println("❌ Failed to find service UUID");
    pClient->disconnect();
    return false;
  }
  
  Serial.println("✅ Service found!");
  
  // Get TX characteristic (for writing)
  pTxCharacteristic = pRemoteService->getCharacteristic(BLEUUID(TX_CHAR_UUID));
  if (pTxCharacteristic == nullptr) {
    Serial.println("❌ Failed to find TX characteristic");
    pClient->disconnect();
    return false;
  }
  
  // Get RX characteristic (for notifications)
  pRxCharacteristic = pRemoteService->getCharacteristic(BLEUUID(RX_CHAR_UUID));
  if (pRxCharacteristic == nullptr) {
    Serial.println("❌ Failed to find RX characteristic");
    pClient->disconnect();
    return false;
  }
  
  // Register for notifications
  if (pRxCharacteristic->canNotify()) {
    pRxCharacteristic->registerForNotify(bleNotifyCallback);
    Serial.println("✅ Registered for notifications!");
  } else {
    Serial.println("❌ Characteristic doesn't support notifications!");
    pClient->disconnect();
    return false;
  }
  
  deviceConnected = true;
  stats.lastConnectionTime = millis();
  return true;
}

void BLEOBDClient::disconnect() {
  if (pClient && deviceConnected) {
    pClient->disconnect();
    deviceConnected = false;
    updateConnectionState(DISCONNECTED);
  }
}

void BLEOBDClient::initializeOBD() {
  Serial.println("🔧 Initializing OBD2 connection...");
  updateConnectionState(INITIALIZING);
  
  // Clear any existing commands
  resetCommandQueue();
  
  // Send initialization commands with delays
  delay(500);
  sendCommand("ATZ");      // Reset
  delay(1500);             // Wait for reset
  sendCommand("ATE0");     // Echo off
  delay(200);
  sendCommand("ATL0");     // Linefeeds off
  delay(200);
  sendCommand("ATS0");     // Spaces off
  delay(200);
  sendCommand("ATSP0");    // Auto protocol
  delay(500);
  
  Serial.println("✅ OBD2 initialization complete!");
  updateConnectionState(CONNECTED);
}

void BLEOBDClient::setupOBDCommands() {
  Serial.println("📋 Setting up OBD command queue...");
  
  // Add commands to queue (non-blocking like ELMduino)
  addCommand("010C", &obdData.rpm, parseRPM);           // Engine RPM
  addCommand("010D", &obdData.speed, parseSpeed);       // Vehicle Speed
  addCommand("0105", &obdData.coolantTemp, parseTemperature); // Coolant Temp
  addCommand("015C", &obdData.oilTemp, parseTemperature);     // Oil Temp
  addCommand("012F", &obdData.fuelLevel, parsePercentage);    // Fuel Level
  addCommand("0111", &obdData.throttlePos, parsePercentage);  // Throttle Position
  addCommand("0104", &obdData.engineLoad, parsePercentage);   // Engine Load
  addCommand("0110", &obdData.airflowRate, parseAirflow);     // Airflow Rate
  
  Serial.println("✅ Command queue ready with " + String(commandQueue.size()) + " commands");
}

void BLEOBDClient::addCommand(String cmd, float* target, bool (*parser)(String, float*)) {
  OBDCommand newCmd;
  newCmd.command = cmd;
  newCmd.targetVariable = target;
  newCmd.parseFunction = parser;
  newCmd.timeout = defaultTimeout;
  newCmd.completed = false;
  newCmd.sentTime = 0;
  commandQueue.push_back(newCmd);
}

void BLEOBDClient::processCommandQueue() {
  static unsigned long lastCommandCheck = 0;
  
  if (millis() - lastCommandCheck < 100) return; // Throttle command processing
  lastCommandCheck = millis();
  
  if (commandQueue.empty()) return;
  
  // Process current command if completed
  if (currentCommandIndex < commandQueue.size() && commandQueue[currentCommandIndex].completed) {
    OBDCommand& cmd = commandQueue[currentCommandIndex];
    
    if (cmd.rawResponse.length() > 0 && !cmd.rawResponse.startsWith("NO DATA")) {
      if (cmd.parseFunction && cmd.targetVariable) {
        if (cmd.parseFunction(cmd.rawResponse, cmd.targetVariable)) {
          stats.successfulCommands++;
          obdData.lastUpdate = millis();
          
          // Update response time statistics
          unsigned long responseTime = millis() - cmd.sentTime;
          if (stats.averageResponseTime == 0) {
            stats.averageResponseTime = responseTime;
          } else {
            stats.averageResponseTime = (stats.averageResponseTime + responseTime) / 2;
          }
          
          if (verboseLogging) {
            Serial.println("✅ Parsed " + cmd.command + ": " + String(*cmd.targetVariable));
          }
        } else {
          stats.failedCommands++;
          if (debugMode) {
            Serial.println("❌ Parse failed for: " + cmd.command);
          }
        }
      }
    } else {
      stats.failedCommands++;
      if (debugMode) {
        Serial.println("❌ No data for: " + cmd.command);
      }
    }
    
    // Move to next command
    currentCommandIndex++;
    if (currentCommandIndex >= commandQueue.size()) {
      currentCommandIndex = 0; // Loop back to start
    }
    
    // Reset command for next cycle
    cmd.completed = false;
    cmd.rawResponse = "";
    cmd.sentTime = 0;
  }
  
  // Send next command if not waiting
  if (!waitingForResponse && currentCommandIndex < commandQueue.size()) {
    OBDCommand& cmd = commandQueue[currentCommandIndex];
    sendCommand(cmd.command);
    waitingForResponse = true;
    lastCommandTime = millis();
    cmd.sentTime = millis();
    stats.totalCommands++;
    
    if (verboseLogging) {
      Serial.println("📤 Sent: " + cmd.command);
    }
  }
}

void BLEOBDClient::sendCommand(String command) {
  if (deviceConnected && pTxCharacteristic) {
    command += "\r"; // Add carriage return
    pTxCharacteristic->writeValue(command.c_str(), command.length());
    
    if (debugMode) {
      Serial.println("📤 Sent: " + command.substring(0, command.length()-1));
    }
  }
}

void BLEOBDClient::processIncomingData(String data) {
  incomingData += data;
  
  if (verboseLogging) {
    Serial.println("📨 Raw BLE data: '" + data + "'");
    Serial.println("📋 Buffer: '" + incomingData + "'");
  }
  
  // Check if we have a complete response (ends with >)
  if (incomingData.indexOf('>') != -1) {
    // Extract the response (everything before the >)
    int promptPos = incomingData.indexOf('>');
    String completeResponse = incomingData.substring(0, promptPos);
    completeResponse.trim();
    
    if (debugMode && completeResponse.length() > 0) {
      Serial.println("✅ Complete response: '" + completeResponse + "'");
    }
    
    // Process the response
    if (waitingForResponse && currentCommandIndex < commandQueue.size()) {
      OBDCommand& cmd = commandQueue[currentCommandIndex];
      cmd.rawResponse = completeResponse;
      cmd.completed = true;
      waitingForResponse = false;
      
      unsigned long responseTime = millis() - lastCommandTime;
      
      if (debugMode) {
        Serial.println("🎯 Command completed in " + String(responseTime) + "ms");
      }
    }
    
    // Clear the buffer
    incomingData = "";
  }
}

void BLEOBDClient::handleTimeout() {
  if (currentCommandIndex < commandQueue.size()) {
    OBDCommand& cmd = commandQueue[currentCommandIndex];
    Serial.println("⏰ Command timeout: " + cmd.command);
    stats.failedCommands++;
    waitingForResponse = false;
    cmd.completed = true;
    cmd.rawResponse = "TIMEOUT";
  }
}

void BLEOBDClient::resetCommandQueue() {
  commandQueue.clear();
  currentCommandIndex = 0;
  waitingForResponse = false;
  incomingData = "";
}

void BLEOBDClient::updateConnectionState(ConnectionState newState) {
  if (newState != connectionState) {
    connectionState = newState;
    lastStateChange = millis();
    
    String stateNames[] = {"DISCONNECTED", "SCANNING", "CONNECTING", "INITIALIZING", "CONNECTED", "ERROR"};
    if (debugMode) {
      Serial.println("🔄 State: " + stateNames[newState]);
    }
  }
}

void BLEOBDClient::displayOBDData() {
  if (!deviceConnected) return;
  
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("🚗 OBD2 DATA UPDATE");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("🔄 RPM: " + String(obdData.rpm, 0) + " rpm");
  Serial.println("🏃 Speed: " + String(obdData.speed, 0) + " km/h");
  Serial.println("🌡️  Coolant: " + String(obdData.coolantTemp, 1) + "°C");
  Serial.println("🛢️  Oil: " + String(obdData.oilTemp, 1) + "°C");
  Serial.println("⛽ Fuel: " + String(obdData.fuelLevel, 1) + "%");
  Serial.println("💨 Throttle: " + String(obdData.throttlePos, 1) + "%");
  Serial.println("🔧 Load: " + String(obdData.engineLoad, 1) + "%");
  Serial.println("🌬️  Airflow: " + String(obdData.airflowRate, 2) + " g/s");
  
  unsigned long dataAge = millis() - obdData.lastUpdate;
  Serial.println("⏰ Data age: " + String(dataAge) + "ms");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
}

void BLEOBDClient::displayStatistics() {
  float successRate = getSuccessRate();
  
  Serial.println("📊 STATISTICS:");
  Serial.println("   📨 Total Commands: " + String(stats.totalCommands));
  Serial.println("   ✅ Successful: " + String(stats.successfulCommands));
  Serial.println("   ❌ Failed: " + String(stats.failedCommands));
  Serial.println("   📈 Success Rate: " + String(successRate, 1) + "%");
  Serial.println("   ⚡ Avg Response: " + String(stats.averageResponseTime) + "ms");
  
  if (deviceConnected) {
    unsigned long currentUptime = getUptime();
    Serial.println("   ⏰ Current Uptime: " + String(currentUptime / 1000) + "s");
  }
  
  Serial.println("   🔄 Reconnect Attempts: " + String(stats.reconnectAttempts));
}

void BLEOBDClient::printConnectionInfo() {
  String stateNames[] = {"DISCONNECTED", "SCANNING", "CONNECTING", "INITIALIZING", "CONNECTED", "ERROR"};
  Serial.println("📱 Connection Status: " + stateNames[connectionState]);
  
  if (connectionState == SCANNING) {
    unsigned long scanDuration = millis() - scanStartTime;
    Serial.println("🔍 Scanning for: " + deviceName + " (" + String(scanDuration/1000) + "s)");
  }
}

void BLEOBDClient::printSystemInfo() {
  Serial.println("🔧 System Information:");
  Serial.println("   📋 ESP32 Chip: " + String(ESP.getChipModel()));
  Serial.println("   🔢 Revision: " + String(ESP.getChipRevision()));
  Serial.println("   💾 Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("   ⏰ CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
  Serial.println();
}

float BLEOBDClient::getSuccessRate() const {
  if (stats.totalCommands == 0) return 0.0;
  return (stats.successfulCommands * 100.0) / stats.totalCommands;
}

unsigned long BLEOBDClient::getUptime() const {
  if (!deviceConnected) return 0;
  return millis() - stats.lastConnectionTime;
}

// Static parsing functions
bool BLEOBDClient::parseRPM(String response, float* value) {
  if (response.length() < 8) return false;
  
  response.replace(" ", "");
  if (!response.startsWith("410C")) return false;
  
  String hex_a = response.substring(4, 6);
  String hex_b = response.substring(6, 8);
  
  int a = strtol(hex_a.c_str(), NULL, 16);
  int b = strtol(hex_b.c_str(), NULL, 16);
  
  *value = ((a * 256) + b) / 4.0;
  return true;
}

bool BLEOBDClient::parseSpeed(String response, float* value) {
  if (response.length() < 6) return false;
  
  response.replace(" ", "");
  if (!response.startsWith("410D")) return false;
  
  String hex_val = response.substring(4, 6);
  *value = strtol(hex_val.c_str(), NULL, 16);
  return true;
}

bool BLEOBDClient::parseTemperature(String response, float* value) {
  if (response.length() < 6) return false;
  
  response.replace(" ", "");
  String pid = response.substring(2, 4);
  
  if (pid != "05" && pid != "5C") return false;
  
  String hex_val = response.substring(4, 6);
  *value = strtol(hex_val.c_str(), NULL, 16) - 40;
  return true;
}

bool BLEOBDClient::parsePercentage(String response, float* value) {
  if (response.length() < 6) return false;
  
  response.replace(" ", "");
  String hex_val = response.substring(4, 6);
  *value = (strtol(hex_val.c_str(), NULL, 16) * 100.0) / 255.0;
  return true;
}

bool BLEOBDClient::parseAirflow(String response, float* value) {
  if (response.length() < 8) return false;
  
  response.replace(" ", "");
  if (!response.startsWith("4110")) return false;
  
  String hex_a = response.substring(4, 6);
  String hex_b = response.substring(6, 8);
  
  int a = strtol(hex_a.c_str(), NULL, 16);
  int b = strtol(hex_b.c_str(), NULL, 16);
  
  *value = ((a * 256) + b) / 100.0;
  return true;
}

bool BLEOBDClient::parseVoltage(String response, float* value) {
  // Implementation depends on specific voltage PID used
  // This is a placeholder for voltage parsing
  *value = 12.6; // Default voltage
  return true;
}

// BLE Callbacks Implementation
void OBDClientCallbacks::onConnect(BLEClient* pClient) {
  Serial.println("🎉 BLE Connected to OBD2 device!");
  // Connection state will be updated in main connectToDevice function
}

void OBDClientCallbacks::onDisconnect(BLEClient* pClient) {
  g_bleClient->deviceConnected = false;
  g_bleClient->updateConnectionState(DISCONNECTED);
  
  unsigned long uptime = g_bleClient->getUptime();
  g_bleClient->stats.connectionUptime += uptime;
  
  Serial.println("💔 BLE Disconnected! Uptime was: " + String(uptime) + "ms");
  
  if (g_bleClient->autoReconnect) {
    Serial.println("🔄 Will attempt reconnection...");
    g_bleClient->doScan = true;
  }
}

void OBDScanCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
  if (g_bleClient->verboseLogging) {
    Serial.println("🔍 Found device: " + String(advertisedDevice.getName().c_str()));
  }
  
  bool targetFound = false;
  
  // Check by service UUID first
  if (advertisedDevice.haveServiceUUID() && 
      advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
    Serial.println("✅ Found OBD2 BLE service!");
    targetFound = true;
  }
  // Check by device name
  else if (advertisedDevice.getName() == g_bleClient->deviceName) {
    Serial.println("✅ Found target device by name: " + g_bleClient->deviceName);
    targetFound = true;
  }
  
  if (targetFound) {
    BLEDevice::getScan()->stop();
    g_bleClient->targetDevice = new BLEAdvertisedDevice(advertisedDevice);
    g_bleClient->deviceFound = true;
    g_bleClient->doConnect = true;
    g_bleClient->doScan = false;
  }
}

// BLE notification callback
void bleNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                      uint8_t* pData, size_t length, bool isNotify) {
  String response = "";
  for (int i = 0; i < length; i++) {
    response += (char)pData[i];
  }
  
  if (g_bleClient) {
    g_bleClient->processIncomingData(response);
  }
}
#ifndef BLE_OBD_CLIENT_H
#define BLE_OBD_CLIENT_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <vector>

// BLE UUIDs (Nordic UART Service compatible)
#define SERVICE_UUID    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_CHAR_UUID    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Write to this
#define RX_CHAR_UUID    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // Notifications from this

// OBD2 Data structure
struct OBDData {
  float rpm = 0.0;
  float speed = 0.0;
  float coolantTemp = 0.0;
  float oilTemp = 0.0;
  float fuelLevel = 0.0;
  float throttlePos = 0.0;
  float engineLoad = 0.0;
  float airflowRate = 0.0;
  float boostPressure = 0.0;
  float voltage = 0.0;
  int dtcCount = 0;
  bool engineRunning = false;
  unsigned long lastUpdate = 0;
};

// Command structure for non-blocking operations
struct OBDCommand {
  String command;
  String expectedResponse;
  float* targetVariable;
  bool (*parseFunction)(String response, float* value);
  unsigned long timeout;
  bool completed;
  String rawResponse;
  unsigned long sentTime;
};

// Connection states
enum ConnectionState {
  DISCONNECTED,
  SCANNING,
  CONNECTING,
  INITIALIZING,
  CONNECTED,
  ERROR_STATE
};

// Statistics structure
struct Statistics {
  unsigned long totalCommands = 0;
  unsigned long successfulCommands = 0;
  unsigned long failedCommands = 0;
  unsigned long averageResponseTime = 0;
  unsigned long connectionUptime = 0;
  unsigned long lastConnectionTime = 0;
  unsigned long reconnectAttempts = 0;
};

// Main BLE OBD Client class
class BLEOBDClient {
public:
  // Constructor
  BLEOBDClient();
  
  // Initialization
  void begin(String targetDeviceName = "OBD2_Simulator_BLE");
  
  // Main loop processing
  void loop();
  
  // Connection management
  bool connectToDevice();
  void disconnect();
  void startScan();
  
  // OBD2 initialization and commands
  void initializeOBD();
  void setupOBDCommands();
  void addCommand(String cmd, float* target, bool (*parser)(String, float*));
  void processCommandQueue();
  void sendCommand(String command);
  
  // Data access
  OBDData getCurrentData() const { return obdData; }
  Statistics getStatistics() const { return stats; }
  ConnectionState getConnectionState() const { return connectionState; }
  
  // Configuration
  void setDebugMode(bool enabled) { debugMode = enabled; }
  void setVerboseLogging(bool enabled) { verboseLogging = enabled; }
  void setAutoReconnect(bool enabled) { autoReconnect = enabled; }
  void setTimeout(unsigned long timeoutMs) { defaultTimeout = timeoutMs; }
  
  // Status checks
  bool isConnected() const { return deviceConnected; }
  float getSuccessRate() const;
  unsigned long getUptime() const;
  
  // Display methods
  void displayOBDData();
  void displayStatistics();
  void printConnectionInfo();
  
  // Parsing functions (static for use in function pointers)
  static bool parseRPM(String response, float* value);
  static bool parseSpeed(String response, float* value);
  static bool parseTemperature(String response, float* value);
  static bool parsePercentage(String response, float* value);
  static bool parseVoltage(String response, float* value);
  static bool parseAirflow(String response, float* value);
  
private:
  // Connection components
  BLEClient* pClient = nullptr;
  BLERemoteCharacteristic* pTxCharacteristic = nullptr;
  BLERemoteCharacteristic* pRxCharacteristic = nullptr;
  BLEAdvertisedDevice* targetDevice = nullptr;
  BLEScan* pBLEScan = nullptr;
  
  // Connection state
  bool deviceConnected = false;
  bool deviceFound = false;
  bool doConnect = false;
  bool doScan = false;
  ConnectionState connectionState = DISCONNECTED;
  
  // Data storage
  OBDData obdData;
  Statistics stats;
  
  // Command management
  std::vector<OBDCommand> commandQueue;
  int currentCommandIndex = 0;
  unsigned long lastCommandTime = 0;
  bool waitingForResponse = false;
  String incomingData = "";
  
  // Configuration
  String deviceName = "OBD2_Simulator_BLE";
  bool debugMode = true;
  bool verboseLogging = false;
  bool autoReconnect = true;
  unsigned long defaultTimeout = 2000;
  
  // Timing
  unsigned long lastStateChange = 0;
  unsigned long lastDataDisplay = 0;
  unsigned long lastStatsDisplay = 0;
  unsigned long scanStartTime = 0;
  
  // Private methods
  void updateConnectionState(ConnectionState newState);
  void resetCommandQueue();
  void printSystemInfo();
  void handleTimeout();
  void processIncomingData(String data);
  
  // Friend classes for callbacks
  friend class OBDClientCallbacks;
  friend class OBDScanCallbacks;
  friend void bleNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                               uint8_t* pData, size_t length, bool isNotify);
};

// Global instance pointer for callbacks
extern BLEOBDClient* g_bleClient;

// BLE Client callbacks - using unique class names
class OBDClientCallbacks : public BLEClientCallbacks {
public:
  void onConnect(BLEClient* pClient) override;
  void onDisconnect(BLEClient* pClient) override;
};

// BLE Device scan callbacks - using unique class names
class OBDScanCallbacks: public BLEAdvertisedDeviceCallbacks {
public:
  void onResult(BLEAdvertisedDevice advertisedDevice) override;
};

// BLE notification callback function
void bleNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                      uint8_t* pData, size_t length, bool isNotify);

#endif // BLE_OBD_CLIENT_H
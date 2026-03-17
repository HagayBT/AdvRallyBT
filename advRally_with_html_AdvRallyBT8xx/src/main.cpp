char UNITNAME[] = "AdvRallyBT8xx";
const char* wifiPassword = "12345678";
const bool isAdventure = true; // Enduro is 5 functions (false), Adventure is 8 functions (true)
static const unsigned long keyCombinationActiveFunctionsAfter = 3000; // 3s
static const unsigned long turnWifiOffAfterIdleFor = 120000; // 2 minutes

#define PIN_UP       26
#define PIN_DOWN     18
#define PIN_PUSH     19
#define PIN_RIGHT    21
#define PIN_LEFT     22
#define PIN_TGLUP    3
#define PIN_TGLDOWN  1
#define PIN_BTN      0
#define PIN_UNPAIRE  39

#define PIN_LED      25
#define PIN_ILED     27

/****** Do not change code after this line ******/
/************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Adafruit_NeoPixel.h>
#include <BleKeyboard.h>
#include <NimBLEDevice.h> // For clearing BLE bonds on factory reset
#include <DNSServer.h>


// Familys
// Family 1: Uppercase
// Family 2: Lowercase
// Family 3: Digits
// Family 4: Special Characters
// Family 5: Media Keys
// Family 6: Custom Keys

const char family1[27] = {'1','A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
const char family2[27] = {'2','a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
const char family3[11] = {'3','1','2','3','4','5','6','7','8','9','0'};
const char family4[31] = {'4','!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '-', '+', '_', '=', '[', ']', '{', '}', '|', '\\', ';', ':', '\'', '"', ',', '.', '/', '<', '>', '?'};
const uint8_t* family5[17] = {(uint8_t*)"5",KEY_MEDIA_NEXT_TRACK,KEY_MEDIA_PREVIOUS_TRACK,KEY_MEDIA_STOP,KEY_MEDIA_PLAY_PAUSE,KEY_MEDIA_MUTE,
                KEY_MEDIA_VOLUME_UP,KEY_MEDIA_VOLUME_DOWN,KEY_MEDIA_WWW_HOME,KEY_MEDIA_LOCAL_MACHINE_BROWSER,KEY_MEDIA_CALCULATOR,KEY_MEDIA_WWW_BOOKMARKS,
                KEY_MEDIA_WWW_SEARCH,KEY_MEDIA_WWW_STOP,KEY_MEDIA_WWW_BACK,KEY_MEDIA_CONSUMER_CONTROL_CONFIGURATION,KEY_MEDIA_EMAIL_READER};
const uint8_t family6[34] = {6,KEY_LEFT_CTRL,KEY_LEFT_SHIFT,KEY_LEFT_ALT,KEY_LEFT_GUI,KEY_UP_ARROW,KEY_DOWN_ARROW,KEY_LEFT_ARROW,KEY_RIGHT_ARROW,
                KEY_BACKSPACE,KEY_TAB,KEY_RETURN,KEY_ESC,KEY_INSERT,KEY_PRTSC,KEY_DELETE,KEY_PAGE_UP,KEY_PAGE_DOWN,KEY_HOME,KEY_END,KEY_CAPS_LOCK,
                KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6,KEY_F7,KEY_F8,KEY_F9,KEY_F10,KEY_F11,KEY_F12};

// EEPROM
static const int EEPROM_SIZE = 64; // plenty of space
static const int MODE_BLOB_SIZE = 19;
static const int addr_magic = 0; // if not R reset to defaults
static const int addr_mode = 1; // current active mode 1..3
static const int addr_mode1 = 2;  // 2..20
static const int addr_mode2 = addr_mode1 + MODE_BLOB_SIZE; // 21..39
static const int addr_mode3 = addr_mode2 + MODE_BLOB_SIZE; // 40..58
static const int addr_led_brightness = addr_mode3 + MODE_BLOB_SIZE; // 0..100 percent

// Default blobs per agreed schema
// Mode1: Up VolUp, Down VolDown, Left Prev, Right Next, Push Play/Pause, Push2 Enter, TglUp VolUp, TglDown VolDown, Btn Mute, flags 0x00
// Mode2: Up UpArrow, Down DownArrow, Left LeftArrow, Right RightArrow, Push 'c' (Lowercase), Push2 Enter (Special), TglUp '+', TglDown '-', Btn 'd', flags 0x01
// Mode3: Up 'B', Down 'T', Left '&', Right Media Selection, Push Escape, Push2 'A', TglUp 'l', TglDown 'l', Btn '8', flags 0x01
static const int DEFAULT_MODE1[MODE_BLOB_SIZE] = {5,6, 5,7, 5,2, 5,1, 5,4, 5,6, 5,7, 5,5, 5,15, 0};
static const int DEFAULT_MODE2[MODE_BLOB_SIZE] = {6,5, 6,6, 6,7, 6,8, 2,3, 4,12, 4,11, 2,4, 6,11, 1};
static const int DEFAULT_MODE3[MODE_BLOB_SIZE] = {1,2, 2,20, 4,7, 5,15, 6,12, 2,12, 2,12, 3,8, 1,1, 1};

// Access point
WebServer server(80);
unsigned long WebLastUseMs = 0;
DNSServer dnsServer;

// --- BLE Keyboard (NimBLE) ---
static BleKeyboard bleKeyboard; // (UNITNAME, "AdvRally", 100);
int btnDelay = 500;               // limitations of bouetooth and connected devices
//int pinArray[9] = {PIN_UP, PIN_DOWN, PIN_PUSH, PIN_RIGHT, PIN_LEFT, PIN_TGLUP, PIN_TGLDOWN, PIN_BTN, PIN_UNPAIRE};
int pinArray[9] = {PIN_UP, PIN_DOWN,  PIN_LEFT, PIN_RIGHT, PIN_PUSH,  PIN_TGLUP, PIN_TGLDOWN, PIN_BTN, PIN_UNPAIRE};
int milArray[9] = {0,0,0,0,0,0,0,0,0};
int preArray[9] = {0,0,0,0,0,0,0,0,0};
int milPin = millis();
int indArray = sizeof(pinArray) / sizeof(int);
int keyMode = 1;
int ActiveModeKeys[MODE_BLOB_SIZE] = {3,10, 3,1, 3,2, 3,3, 3,4, 3,5, 3,6, 3,7, 3,8, 0};
bool bleConnected = false;

// Leds
int led_state = HIGH;
int led_last_state = HIGH;
int led_brightness = 70;
int led_rgb[3] = {0, 255, 0};
int led_millis = millis();
// Onboard SK6812 (RGB, 1 pixel) on PIN_ILED
static Adafruit_NeoPixel ILED(1, PIN_ILED, NEO_GRB + NEO_KHZ800);

// Combo mode
bool comboMode = false;
unsigned long comboStartMs = 0;
unsigned long comboLastActionMs = 0;
const unsigned long comboTimeoutMs = 5000; // 5s timeout
unsigned long comboBlinkTickMs = 0;
const unsigned long comboBlinkPeriodMs = 150; // fast blink
bool comboBlinkOn = false;

// ******************************************************************************** //

static void applyLed(int iState) { 
  if (led_brightness < 0) led_brightness = 0;
  if (led_brightness > 100) led_brightness = 100;
  int duty = (led_brightness * 255 + 50) / 100; // rounded
  led_rgb[1] = duty;
  if (WiFi.status() == WL_CONNECTED) { led_rgb[0] = duty; }
  if (iState == LOW) { duty = 0; }
  led_rgb[2] = duty;
  analogWrite(PIN_LED, duty); 

  ledcWrite(0, 255);
  ILED.setPixelColor(0, ILED.Color(led_rgb[0], led_rgb[1], led_rgb[2]));
  ILED.show();

  // Keep last-state in sync so callers can avoid redundant updates
  led_last_state = iState;
  Serial.println("LED State: " + String(iState));
}

// EEPROM Functions //////////////////////////////
static void readInt(int addr, int value) {
  value = (EEPROM.read(addr));
}

static void writeInt(int addr, int value) {
  if (EEPROM.read(addr) != value) {
    EEPROM.write(addr, value);
    EEPROM.commit();
  }
} //Avoid unnecessary writes to prolong EEPROM lifespan

static void writeDefaults () {
  writeInt(addr_magic, 'R');
  writeInt(addr_mode, keyMode);
  for (int i = 0; i < MODE_BLOB_SIZE; i++) {
    writeInt(addr_mode1 + i, DEFAULT_MODE1[i]);
  }
  for (int i = 0; i < MODE_BLOB_SIZE; i++) {
    writeInt(addr_mode2 + i, DEFAULT_MODE2[i]);
  }
  for (int i = 0; i < MODE_BLOB_SIZE; i++) {
    writeInt(addr_mode3 + i, DEFAULT_MODE3[i]);
  }
  writeInt(addr_led_brightness, 70);

  Serial.println("EEPROM contents: ");
  Serial.print("Magic: "); Serial.println(EEPROM.read(addr_magic));
  Serial.print("Mode: "); Serial.println(EEPROM.read(addr_mode));
  Serial.print("Mode1: ");
  for (int i = addr_mode1; i < addr_mode2; i++) { Serial.print(EEPROM.read(i)); Serial.print(","); }
  Serial.println();
  Serial.print("Mode2: ");
  for (int i = addr_mode2; i < addr_mode3; i++) { Serial.print(EEPROM.read(i)); Serial.print(","); }
  Serial.println();
  Serial.print("Mode3: ");
  for (int i = addr_mode3; i < addr_led_brightness; i++) { Serial.print(EEPROM.read(i)); Serial.print(","); }
  Serial.println();
  Serial.print("LED Brightness: "); Serial.println(EEPROM.read(addr_led_brightness));
  Serial.println();
}

void loadActiveModeToCache () {
  int base = addr_mode1;
  if (keyMode == 2) base = addr_mode2;
  if (keyMode == 3) base = addr_mode3;

  Serial.print("Loading mode: "); Serial.println(keyMode);

  for (int i = 0; i < MODE_BLOB_SIZE; i++) {
    ActiveModeKeys[i] = EEPROM.read(i+base);
  }

  led_state = LOW; applyLed(led_state); delay(300);
  for (int i = 0; i < keyMode - 1; ++i) {
    led_state = HIGH; applyLed(led_state); delay(100);
    led_state = LOW; applyLed(led_state); delay(200);
  }
}

// bleKeyboard Functions //////////////////////////////////
void keyDown(int location) {
  if (location == 8) {return;}
  int familyLocation = location * 2;
  int valueLocation = familyLocation + 1;
  switch (ActiveModeKeys[familyLocation]) {
    case 1: bleKeyboard.press(family1[ActiveModeKeys[valueLocation]]); break;
    case 2: bleKeyboard.press(family2[ActiveModeKeys[valueLocation]]); break;
    case 3: bleKeyboard.press(family3[ActiveModeKeys[valueLocation]]); break;
    case 4: bleKeyboard.press(family4[ActiveModeKeys[valueLocation]]); break;
    case 5: bleKeyboard.press(family5[ActiveModeKeys[valueLocation]]); break;
    case 6: bleKeyboard.press(family6[ActiveModeKeys[valueLocation]]); break;
  }
  Serial.print("Key Down: "); Serial.print(location);
  Serial.print(", Family: "); Serial.print(ActiveModeKeys[familyLocation]);
  Serial.print(", Value: "); Serial.println(ActiveModeKeys[valueLocation]);

  if ((ActiveModeKeys[18] == 1) and (location == 4)) {
    int familyLocation = 16;
    int valueLocation = familyLocation + 1;
    switch (ActiveModeKeys[familyLocation]) {
      case 1: bleKeyboard.press(family1[ActiveModeKeys[valueLocation]]); break;
      case 2: bleKeyboard.press(family2[ActiveModeKeys[valueLocation]]); break;
      case 3: bleKeyboard.press(family3[ActiveModeKeys[valueLocation]]); break;
      case 4: bleKeyboard.press(family4[ActiveModeKeys[valueLocation]]); break;
      case 5: bleKeyboard.press(family5[ActiveModeKeys[valueLocation]]); break;
      case 6: bleKeyboard.press(family6[ActiveModeKeys[valueLocation]]); break;
    }
    Serial.print("Key Down2: "); Serial.print(location);
    Serial.print(", Family2: "); Serial.print(ActiveModeKeys[familyLocation]);
    Serial.print(", Value2: "); Serial.println(ActiveModeKeys[valueLocation]);
  };
} 

void keyUp(int location) {
  if (location == 8) {return;}
  int familyLocation = location * 2;
  int valueLocation = familyLocation + 1;
  switch (ActiveModeKeys[familyLocation]) {
    case 1: bleKeyboard.release(family1[ActiveModeKeys[valueLocation]]); break;
    case 2: bleKeyboard.release(family2[ActiveModeKeys[valueLocation]]); break;
    case 3: bleKeyboard.release(family3[ActiveModeKeys[valueLocation]]); break;
    case 4: bleKeyboard.release(family4[ActiveModeKeys[valueLocation]]); break;
    case 5: bleKeyboard.release(family5[ActiveModeKeys[valueLocation]]); break;
    case 6: bleKeyboard.release(family6[ActiveModeKeys[valueLocation]]); break;
  }

  if ((ActiveModeKeys[18] == 1) and (location == 4)) {
    // For PUSH2 release, use the same mapping as in keyDown2 (indices 16/17)
    int familyLocation = 16;
    int valueLocation = familyLocation + 1;
    switch (ActiveModeKeys[familyLocation]) {
      case 1: bleKeyboard.release(family1[ActiveModeKeys[valueLocation]]); break;
      case 2: bleKeyboard.release(family2[ActiveModeKeys[valueLocation]]); break;
      case 3: bleKeyboard.release(family3[ActiveModeKeys[valueLocation]]); break;
      case 4: bleKeyboard.release(family4[ActiveModeKeys[valueLocation]]); break;
      case 5: bleKeyboard.release(family5[ActiveModeKeys[valueLocation]]); break;
      case 6: bleKeyboard.release(family6[ActiveModeKeys[valueLocation]]); break;
    }
  };
}

void keyUpAll() { 
  for (int thisPin = 0; thisPin < indArray; thisPin++) {
    if (preArray[thisPin] == 1) {
      preArray[thisPin] = 0; milArray[thisPin] = 0; keyUp(thisPin);
      Serial.print("Key Up "); Serial.println(thisPin);
    }
  } 
}

// Key combo mode
static inline void enterComboMode() { comboMode = true; comboStartMs = comboLastActionMs = millis(); comboBlinkTickMs = 0; comboBlinkOn = false; keyUpAll(); }
static inline void exitComboMode() { comboMode = false; applyLed(led_state); } // restore steady LED
static inline void comboBlinkStep() { if (!comboMode) return; unsigned long now = millis(); if (now - comboBlinkTickMs >= comboBlinkPeriodMs) { comboBlinkTickMs = now; comboBlinkOn = !comboBlinkOn; applyLed(comboBlinkOn ? HIGH : LOW); } }


// Key Combinations //////////////////////////////////////////////
static void performFactoryReset() { 
  Serial.println("[FACTORY RESET] Starting: blink x10, clear BLE bonds, wipe EEPROM, restart");
  // Rapid blink 10 times
  for (int i = 0; i < 10; ++i) {
    led_state = HIGH; applyLed(led_state); delay(100);
    led_state = LOW; applyLed(led_state); delay(100);
  }
  // Clear all BLE bonded devices (NimBLE)
  NimBLEDevice::deleteAllBonds();
  // Wipe EEPROM contents
  for (int i = 0; i < EEPROM_SIZE; ++i) EEPROM.write(i, 0x00);
  EEPROM.commit();
  delay(50);
  Serial.println("[FACTORY RESET] Done. Restarting...");
  ESP.restart(); 
}

static void changeMode(bool up) {
  if (up) {keyMode++;} else {keyMode--;}
  if (keyMode < 1) keyMode = 1;
  if (keyMode > 3) keyMode = 3;
  EEPROM.write(addr_mode, keyMode);
  EEPROM.commit();
  Serial.print("Mode changed to: "); Serial.println(keyMode);
  loadActiveModeToCache();
}

static void changeBrightness(bool up) {
  if (up) {led_brightness += 10;} else {led_brightness -= 10;}
  if (led_brightness < 0) led_brightness = 0;
  if (led_brightness > 100) led_brightness = 100;
  EEPROM.write(addr_led_brightness, led_brightness);
  EEPROM.commit();
  Serial.print("LED Brightness changed to: "); Serial.println(led_brightness);
  led_state = HIGH; applyLed(led_state); delay(50);
  led_state = LOW; applyLed(led_state); delay(50);
}

// WIFI Section ////////////////////////////////////////
static void markWebActivity() { WebLastUseMs = millis(); }

static void sendJson(int code, const String &json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", json);
}

void handleRoot() {
  markWebActivity();
  File f = SPIFFS.open("/index.html", "r");
  if (!f) {
  server.send(404, "text/plain", "index.html not found");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.streamFile(f, "text/html");
  f.close();
}


static void handleUnit() {
  markWebActivity();
  String json = "{\"unitname\":\"";
  json += UNITNAME;
  json += "\"}";
  sendJson(200, json);
}

void handleLedGet() {
  markWebActivity();
  int pct = led_brightness;
  if (pct < 0 || pct > 100) pct = 70;
  String json = "{\"brightness\":" + String(pct) + "}";
  sendJson(200, json);
}

void handleLedSet() {
  markWebActivity();
  if (!server.hasArg("brightness")) {
    sendJson(400, "{\"ok\":false,\"error\":\"Missing brightness\"}");
    return;
  }
  int pct = server.arg("brightness").toInt();
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  writeInt(addr_led_brightness, pct);
  EEPROM.write(addr_led_brightness, pct);
  EEPROM.commit();
  led_brightness = pct;
  applyLed(true);
  String json = "{\"ok\":true,\"brightness\":" + String(pct) + "}";
  sendJson(200, json);
}

void handleSetActiveMode() {
  markWebActivity();
  if (!server.hasArg("keyMode")) {
    sendJson(400, "{\"ok\":false,\"error\":\"Missing mode\"}");
    return;
  }
  int mode = server.arg("keyMode").toInt();
  if (mode < 1 || mode > 3) {
    sendJson(400, "{\"ok\":false,\"error\":\"Invalid mode\"}");
    return;
  }
  keyMode = mode;
  writeInt(addr_mode, (uint8_t)keyMode);
  loadActiveModeToCache();
  String json = "{\"ok\":true,\"mode\":" + String(mode) + "}";
  sendJson(200, json);
}

void handleGetActiveMode() {
  markWebActivity();
  String json = "{\"keyMode\":" + String(keyMode) + "}";
  sendJson(200, json);
}

bool splitBlob(const String& s,int out[MODE_BLOB_SIZE]) {
  int idx = 0;
  int start = 0;
  while (idx < MODE_BLOB_SIZE) {
    int comma = s.indexOf(',', start);
    String part;
    if (comma == -1) {
      part = s.substring(start);
    } else {
      part = s.substring(start, comma);
    }
    part.trim();
    if (part.length() == 0) return false;
    out[idx] = part.toInt();
    if (out[idx] < 0) return false;
    start = comma + 1;
    if (comma == -1) break;
    idx++;
  }
  return (idx == MODE_BLOB_SIZE - 1);
}

void handleGetMode() {
  markWebActivity();
  int idx = 1;
  if (server.hasArg("idx")) {
    idx = server.arg("idx").toInt();
  }
  if (idx < 1 || idx > 3) {
    sendJson(400, "{\"ok\":false,\"error\":\"Invalid idx\"}");
    return;
  }
  int base = (idx == 1) ? addr_mode1 : (idx == 2 ? addr_mode2 : addr_mode3);
  String json = "{\"ok\":true,\"blob\":\"";
  for (int i = 0; i < MODE_BLOB_SIZE; i++) {
    if (i > 0) json += ",";
    json += String(EEPROM.read(base + i));
  }
  json += "\"}";
  sendJson(200, json);
}

void handleSetMode() {
  markWebActivity();
  // expects idx=1..3 and blob=38-hex chars (19 bytes)
  if (!server.hasArg("idx") || !server.hasArg("blob")) {
    sendJson(400, "{\"ok\":false,\"error\":\"Missing idx/blob\"}");
    return;
  }
  int idx = server.arg("idx").toInt();
  String hex = server.arg("blob");
  if (idx < 1 || idx > 3) {
    sendJson(400, "{\"ok\":false,\"error\":\"Invalid idx\"}");
    return;
  }
  if (!splitBlob(hex, ActiveModeKeys)) {
    sendJson(400, "{\"ok\":false,\"error\":\"Invalid blob content (expected 19 comma-separated integers)\"}");
    return;
  }
  
  int addr = (idx == 1) ? addr_mode1 : (idx == 2 ? addr_mode2 : addr_mode3);
  for (int i = 0; i < MODE_BLOB_SIZE; i++) {
    writeInt(addr + i, ActiveModeKeys[i]);
  }
  // If we just updated the active mode, reload cache
  if (idx == keyMode) loadActiveModeToCache();
  sendJson(200, "{\"ok\":true}");
}

void handleIsAdventure() {
  markWebActivity();
  String json = "{\"isAdventure\":";
  json += (isAdventure ? "true" : "false");
  json += "}";
  sendJson(200, json);
}

void handlePing() {
  sendJson(200, "{\"ping\":true}");
}

static void startWifi() {
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    Serial.println("WiFi already connected.");
    return;
  }
  Serial.println("Starting WiFi...");
  SPIFFS.begin(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(UNITNAME, wifiPassword);
  Serial.print("AP IP address: "); Serial.println(WiFi.softAPIP());
  // DNS captive portal
  dnsServer.start(53, "*", WiFi.softAPIP());

  WebLastUseMs = millis();

  server.on("/", handleRoot);
  server.on("/getActiveMode", handleGetActiveMode);
  server.on("/setActiveMode", handleSetActiveMode);
  server.on("/getMode", HTTP_GET, handleGetMode);
  server.on("/setMode", HTTP_POST, handleSetMode);
  server.on("/getLed", HTTP_GET, handleLedGet);
  server.on("/setLed", HTTP_POST, handleLedSet);
  server.on("/unit", HTTP_GET, handleUnit);
  server.on("/isAdventure", handleIsAdventure);
  server.on("/ping", HTTP_GET, handlePing);
  
  // Basic preflight support
  /*server.on("/mode", HTTP_OPTIONS, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);server.begin();
    applyLed(true);}
  );*/
  server.begin();
}
  // Setup & Loop //////////////////////////////////////
void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  bleKeyboard.setName(UNITNAME);
  bleKeyboard.begin();
  
  for (int thisPin = 0; thisPin < indArray; thisPin++) {
    if (!isAdventure && (pinArray[thisPin] == PIN_TGLUP || pinArray[thisPin] == PIN_TGLDOWN || pinArray[thisPin] == PIN_BTN)) continue;
    Serial.println(pinArray[thisPin]);
    pinMode(pinArray[thisPin], INPUT_PULLUP);
  }
  
  ledcSetup(0, 1000, 8);
  ledcAttachPin(PIN_ILED, 0);
  pinMode(PIN_LED, OUTPUT);
  applyLed(led_state);
  
  EEPROM.begin(EEPROM_SIZE);

  if (EEPROM.read(addr_magic) != 'R') {
    Serial.println("Invalid EEPROM data, writing defaults");
    writeDefaults();
    EEPROM.commit();
  }
  keyMode = EEPROM.read(addr_mode);
  Serial.print("Current Mode: ");
  Serial.println(keyMode);
  
  loadActiveModeToCache();

  led_brightness = EEPROM.read(addr_led_brightness);
  if (led_brightness < 0 || led_brightness > 100) led_brightness = 55;
  applyLed(true);

  startWifi();
    
  Serial.print("LED Brightness: ");
  Serial.println(led_brightness);

  Serial.print("Active Mode: ");
  Serial.println(keyMode);

  Serial.print("Active Mode Keys: ");
  for (int i = 0; i < MODE_BLOB_SIZE; i++) {
    Serial.print(ActiveModeKeys[i]);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println("Setup complete");
}

void loop() {
  if (comboMode) { comboBlinkStep(); if (millis() - comboLastActionMs >= comboTimeoutMs) { exitComboMode(); } }
  // Determine desired LED state based on timers/connection, then apply once if changed
  int desired_led_state = (millis() >= led_millis) ? HIGH : LOW;

  bleConnected = bleKeyboard.isConnected();
  if (!bleConnected) {
    if (millis() > (led_millis + 1000)) {
      desired_led_state = LOW;
      led_millis = millis() + 1000;
    }
  }

  if (!comboMode) { if (desired_led_state != led_last_state) { applyLed(desired_led_state); } led_state = desired_led_state; }

  if (WiFi.softAPgetStationNum() > 0) {
    dnsServer.processNextRequest();
    server.handleClient();
    //Serial.println("Handling client requests");
    if ((millis() - WebLastUseMs) > turnWifiOffAfterIdleFor) {
      Serial.println("Stopping WiFi due to inactivity");
      server.stop();
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_OFF);
      applyLed(led_state);
    }
  } 

  for (int thisPin = 0; thisPin < indArray; thisPin++) {
    // Skip non-existent pins in Enduro mode
    if (!isAdventure && (pinArray[thisPin] == PIN_TGLUP || pinArray[thisPin] == PIN_TGLDOWN || pinArray[thisPin] == PIN_BTN)) continue;
    // Key Down
    if((digitalRead(pinArray[thisPin]) == LOW) & (milArray[thisPin] == 0)) {milArray[thisPin] = millis();}
    if((milArray[thisPin] >= 1) & (millis() - milArray[thisPin] >= 31) & (preArray[thisPin] == 0)){
      preArray[thisPin] = 1;
      if (!comboMode && bleConnected == true) {keyDown(thisPin);}
      Serial.print("Key Down ");
      Serial.println(thisPin);
    }

    // Key Combination Active Functions
    int k1 = 4;
    int k2 = isAdventure ? 7 : 0; // PIN_PUSH + PIN_BTN for Adventure, PIN_PUSH + PIN_UP for Enduro
    if (!comboMode) { if (preArray[k1] == 1 && preArray[k2] == 1 && (millis() - milArray[k1] >= keyCombinationActiveFunctionsAfter) && (millis() - milArray[k2] >= keyCombinationActiveFunctionsAfter)) { enterComboMode(); } }

    // Combo Mode Functions
    if (comboMode) {
      auto consume = [&](int idx){ preArray[idx] = 0; milArray[idx] = 0; };
      if (preArray[0] == 1) { changeBrightness(true); comboLastActionMs = millis(); consume(0); }
      if (preArray[1] == 1) { changeBrightness(false); comboLastActionMs = millis(); consume(1); }
      if (preArray[2] == 1){ changeMode(false); comboLastActionMs = millis(); consume(3); }
      if (preArray[3] == 1) { changeMode(true); comboLastActionMs = millis(); consume(4); }
      if (preArray[4] == 1) { startWifi(); comboLastActionMs = millis(); keyUpAll(); consume(2); }
    }
    // Factory Reset
    if (preArray[8] == 1 && (millis() - milArray[8] >= keyCombinationActiveFunctionsAfter)) { keyUpAll(); performFactoryReset(); }

    // Key Up
    if((digitalRead(pinArray[thisPin]) == HIGH) & (preArray[thisPin] == 1)) {
      if (millis() - milArray[thisPin] >= 31) {
        if (!comboMode && bleConnected == true) {keyUp(thisPin); Serial.print("Key Up "); Serial.println(thisPin);} //prevent ghost press
      }
      preArray[thisPin] = 0;
      milArray[thisPin] = 0;
      led_millis = millis() + 50;
    }
  }
}

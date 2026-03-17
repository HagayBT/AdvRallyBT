/*  Media button Bluetooth controller for rally book navigation by Israeli ADV
 *  Code by Hagay BT as part of Adventure Motorcycles Israel
 *  (CC) Creative Commons licenses - this comment should not be deleted!
 *  https://www.facebook.com/groups/AdventureIL/
 *  
 *  To change mode press vulume up, volume down and play/puse together
 *  
 *  Mode 1 for use with:
 *  Piste Roadbook - Android
 *  Rally Roadbook Reader – iPhone & Android
 *  
 *  Mode 2 for use with OsmAnd
 *  Mode 3 for use with OsmAnd and DMD2 but not working (no dpad)
 *  
 *  Add library “ESP32-BLE-Keyboard-master.zip”
 *  [ESP32-BLE-Keyboard-master@0.3.2]
 *  
 *  Development Board Arduino ESP32 CH9102X (WEMOS LOLIN32, 80MHz, Default, 115200)
 *  
 *  If you have issues with Bluetooth connection use pin 15 (PIN_UNPAIRE) with ground for 3 seconds
 *    will result in resetting all bonded devices
 *  
 *  Unit Name and GPIO ports can be changed
 *  
 *  Tools: Board: ESP32 Arduino: WEMOS LOLIN32
 */

char UNITNAME[] = "ADVMotoIL???";
 
#define PIN_VOLUME_UP       26    // brown
#define PIN_VOLUME_DOWN     18    // yellow
#define PIN_PLAY_PAUSE      19    // white
#define PIN_NEXT_TRACK      21    // blue
#define PIN_PREVIOUS_TRACK  22    // green
#define PIN_LED             25    // red
#define PIN_ILED            27     // on board led pin
#define PIN_UNPAIRE         39    // connect to ground for 3 seconds to reset all bound devices

/****** Do not change code after this line ******/

#include <BleKeyboard.h>
#include <EEPROM.h>
#include <FastLED.h>

const uint8_t* modRally[5] PROGMEM = {KEY_MEDIA_VOLUME_UP,KEY_MEDIA_VOLUME_DOWN,KEY_MEDIA_PLAY_PAUSE,KEY_MEDIA_NEXT_TRACK,KEY_MEDIA_PREVIOUS_TRACK};
const uint8_t modOsmAnd[5] PROGMEM = {KEY_LEFT_ARROW,KEY_RIGHT_ARROW,KEY_RETURN,KEY_UP_ARROW,KEY_DOWN_ARROW};
String txtRally[5] PROGMEM = {"KEY_MEDIA_VOLUME_UP","KEY_MEDIA_VOLUME_DOWN","KEY_MEDIA_PLAY_PAUSE","KEY_MEDIA_NEXT_TRACK","KEY_MEDIA_PREVIOUS_TRACK"};
String txtOsmAnd[5] PROGMEM = {"KEY_LEFT_ARROW","KEY_RIGHT_ARROW","KEY_RETURN","KEY_UP_ARROW","KEY_DOWN_ARROW"};

int btnDelay = 500;               // limitations of bouetooth and connected devices

int pinArray[6] = {PIN_VOLUME_UP,PIN_VOLUME_DOWN,PIN_PLAY_PAUSE,PIN_NEXT_TRACK,PIN_PREVIOUS_TRACK,PIN_UNPAIRE};
int milArray[6] = {0,0,0,0,0,0};
int preArray[6] = {0,0,0,0,0,0};
int milPin = millis();
int milLed = millis();
int indArray = sizeof(pinArray) / sizeof(int);
int led_state = LOW;
int keyMode = 0;

#define EEPROM_SIZE 1
#define NUM_LEDS 1

CRGB leds[NUM_LEDS];

BleKeyboard bleKeyboard;//("Rally Keyboard","Adventure Motorcycles Israel",100);

void setup() {
  Serial.begin(115200);
  Serial.print(UNITNAME);
  Serial.println(" Starting BLE work!");
  bleKeyboard.setName(UNITNAME);
  bleKeyboard.begin();
  for (int thisPin = 0; thisPin < indArray; thisPin++) {
    pinMode(pinArray[thisPin], INPUT_PULLUP);
  }
  pinMode(PIN_LED, OUTPUT);
  //pinMode(PIN_ILED, OUTPUT);
  FastLED.addLeds<SK6812, PIN_ILED, RGB>(leds, NUM_LEDS);

  EEPROM.begin(EEPROM_SIZE);
  keyMode = EEPROM.read(0);
}

void loop() {
  if(bleKeyboard.isConnected()) {
    if(millis() - milLed >= 50) {led_state = HIGH;}
    ledLight(led_state);
    if((preArray[0]==1) & (preArray[1]==1)){nextKeyMode();}
    for (int thisPin = 0; thisPin < indArray; thisPin++) {
      if((digitalRead(pinArray[thisPin]) == LOW) & (milArray[thisPin] == 0)) {milArray[thisPin] = millis();}
      if((milArray[thisPin] >= 1) & (millis() - milArray[thisPin] >= 31) & (preArray[thisPin] == 0)){
        preArray[thisPin] = 1;
        keyDown(thisPin);
      }
      
      if((digitalRead(pinArray[thisPin]) == HIGH) & (milArray[thisPin] >= 1)) {

        Serial.print("Pin - millis - digitalRead");
        Serial.print(thisPin);
        Serial.print(" - ");
        Serial.print(millis() - milArray[thisPin]);
        Serial.print(" - ");
        Serial.println(digitalRead(pinArray[thisPin]));
        
        switch(millis() - milArray[thisPin]){
          case 0 ... 30:
            Serial.print(thisPin);
            Serial.println(" - Ghost Press");
            break;
          case 31 ... 999:
            // Short Press
            keyUp(thisPin);
            break;
           default:
            // long Press  
            if(thisPin < 5) {Serial.print("LONG_");keyUp(thisPin);}
            else if(thisPin == 5) {paireDeviceDelete(1);}
        };
        preArray[thisPin] = 0;
        milArray[thisPin] = 0;
        milLed = millis();
        led_state = LOW;
        ledLight(led_state);
      }
     }
    } else {
    led_state = !led_state;
    ledLight(led_state);
    delay(btnDelay);
    if(digitalRead(PIN_UNPAIRE) == LOW) {
      paireDeviceDelete(1);
      ledLight(HIGH);
      delay(3000);
      }
  }
}

void ledLight(int setState) {
  digitalWrite(PIN_LED, setState);
  //digitalWrite(PIN_ILED,setState);
  if(setState==HIGH){leds[0] = 0xf00000;}
  else{leds[0] = 0x00f000;}
  FastLED.show();
}

void nextKeyMode() {
  for(int i=0;i<5;i++) {keyUp(i);preArray[i] = 0;milArray[i] = 0;}
  //bleKeyboard.releaseAll();
  keyMode += 1;
  if (keyMode > 1) {keyMode = 0;}
  EEPROM.write(0, keyMode);
  EEPROM.commit();
  Serial.print("keyMode changed to: ");
  Serial.println(keyMode);
  /* flush led */
  ledLight(LOW);
  delay(1000);
  for (int i=0;i<(keyMode+1);i++) {
    ledLight(HIGH);
    delay(250);
    ledLight(LOW);
    delay(250);
  }
  delay(2000);
}

void keyDown(int location) {
  if(keyMode == 1) {bleKeyboard.press(modOsmAnd[location]);Serial.print("Down_");Serial.println(txtOsmAnd[location]);}
  else{bleKeyboard.press(modRally[location]);Serial.print("Down_");Serial.println(txtRally[location]);}
  if((keyMode == 1) & (modOsmAnd[location] == KEY_RETURN)) {bleKeyboard.press('c');Serial.println("Down_C");}
}

void keyUp(int location) {
  if(keyMode == 1) {bleKeyboard.release(modOsmAnd[location]);Serial.print("Up_");Serial.println(txtOsmAnd[location]);}
  else {bleKeyboard.release(modRally[location]);Serial.print("Up_");Serial.println(txtRally[location]);}
  if((keyMode == 1) & (modOsmAnd[location] == KEY_RETURN)) {bleKeyboard.release('c');Serial.println("Up_C");}
}
void paireDeviceDelete(int REMOVE_BONDED_DEVICES) {
  Serial.println("reset");
   int dev_num = esp_ble_get_bond_device_num();
    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
    }

    free(dev_list);
}

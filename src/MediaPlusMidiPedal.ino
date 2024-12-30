// This project uses an Adafruit nRF52840 Express to act as a foot pedal to
// control the Ultimate Guitar app on iOS (Android not tested)

// Currently maps the following keys to the "pedals" Ultimate Guitar recognizes:
//  HID_KEY_4 => Ped0
//  HID_KEY_BACKSLASH => Ped1
//  HID_KEY_PERIOD => Ped2
//  HID_KEY_SEMICOLON => Ped3
//  HID_KEY_SLASH => Ped4
//  HID_KEY_GRAVE => Ped5
//  HID_KEY_COMMA => Ped6

#include <bluefruit.h>

BLEDis bledis;
BLEHidAdafruit blehid;

int ledPower = 14;
int ledBattLow = 15;
int ledBluetooth = 16;
bool isConnected = false;

// Long-press threshold (milliseconds)
const unsigned long LONG_PRESS_THRESHOLD = 1000;

const unsigned long DEBOUNCE_MS = 20;

#define VBATPIN A6

const int SWITCH_COUNT = 4;
const int FOOT_SWITCH_1_PIN = 5;
const int FOOT_SWITCH_2_PIN = 6;
const int FOOT_SWITCH_3_PIN = 9;
const int FOOT_SWITCH_4_PIN = 10;

// Foot switch pins
const int footSwitchPins[SWITCH_COUNT] = {FOOT_SWITCH_1_PIN, FOOT_SWITCH_2_PIN, FOOT_SWITCH_3_PIN, FOOT_SWITCH_4_PIN};

bool stableState[SWITCH_COUNT]       = {HIGH, HIGH, HIGH, HIGH};  // Start as "not pressed" (with pull-ups)
bool lastRawReading[SWITCH_COUNT]    = {HIGH, HIGH, HIGH, HIGH};  // Last raw pin reading
unsigned long lastDebounceTime[SWITCH_COUNT] = {0, 0, 0, 0};     // Last time we saw a change
bool isCurrentlyPressed[SWITCH_COUNT]= {false, false, false, false}; // Are we in a pressed state?
unsigned long pressTime[SWITCH_COUNT]    = {0, 0, 0, 0};         // When the switch went down
int normalKeys[SWITCH_COUNT] = {HID_KEY_4, HID_KEY_BACKSLASH, HID_KEY_PERIOD, HID_KEY_SEMICOLON};
int longPressKeys[SWITCH_COUNT] = {HID_KEY_SLASH, HID_KEY_BACKSLASH, HID_KEY_PERIOD, HID_KEY_SEMICOLON};

void setup() 
{
  pinMode(ledPower, OUTPUT);
  pinMode(ledBattLow, OUTPUT);
  pinMode(ledBluetooth, OUTPUT);

  // Initialize each foot switch pin
  for (int i = 0; i < SWITCH_COUNT; i++) {
    pinMode(footSwitchPins[i], INPUT_PULLUP);
  }

  Serial.begin(115200);
  
  Bluefruit.begin();
  Bluefruit.setName("UG_Test_Pedal");
  
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit nRF52840");
  bledis.begin();

  blehid.begin();

  // Set callbacks
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  startAdv();
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void testHIDConsumer(uint16_t code, const char* description) {
  Serial.print("Testing Media Control: ");
  Serial.println(description);
  
  blehid.consumerKeyPress(code);
  delay(10);
  blehid.consumerKeyRelease();
  delay(2000);
}

void testKeyboard(uint8_t key, uint8_t modifier, const char* description) {
  Serial.print("Testing: ");
  Serial.println(description);
  
  blehid.keyPress(modifier, key);
  delay(50);
  blehid.keyRelease();
  delay(3000);  // Longer delay to see effect
}

void connect_callback(uint16_t conn_handle) {
  isConnected = true;
  Serial.println("Connected!");
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  isConnected = false;
  Serial.println("Disconnected!");
  startAdv();
}

void sendKeyCombo(uint8_t keycode, uint8_t modifier) {
  blehid.keyPress(modifier, keycode);
  // delay(10);
  blehid.keyRelease();
}

void testKeyboardNoDelay(uint8_t key, uint8_t modifier, const char* description) {
  Serial.print("Testing: ");
  Serial.println(description);
  
  blehid.keyPress(modifier, key);
  delay(50);
  blehid.keyRelease();
  //delay(3000);  // Longer delay to see effect
}

void loop() 
{
  // If we're here, the device has power so turn on the power (red) LED
  digitalWrite(ledPower, HIGH);

  // If bluetooth has connected, turn on the bluetooth (blue) LED
  // TODO: Blink the blue LED if the device is advertising but not yet connected
  if (isConnected) {
    digitalWrite(ledBluetooth, HIGH);
  } else {
    digitalWrite(ledBluetooth, LOW);
    delay(1000);
    return;
  }

  // Read the battery voltage
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.6;  // Multiply by 3.6V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  // Serial.print("VBat: " ); 
  // Serial.println(measuredvbat);

  // Turn on the battery (yellow) LED if the voltage is low signifying that
  // the device is not plugged in and battery is low
  if(measuredvbat < 3.7) {
    digitalWrite(ledBattLow, HIGH);
  } else {
    digitalWrite(ledBattLow, LOW);
  }

// Check each foot switch in turn
  for (int i = 0; i < SWITCH_COUNT; i++) {
    bool rawReading = digitalRead(footSwitchPins[i]);  // HIGH = not pressed, LOW = pressed
    
    // 1) Detect if the raw reading changed from the last reading
    if (rawReading != lastRawReading[i]) {
      // The pin state just changed; reset the debounce timer
      lastDebounceTime[i] = millis();
    }
    lastRawReading[i] = rawReading; // Update for next iteration

    // 2) If it's been stable at this new state for DEBOUNCE_MS, accept it
    if ((millis() - lastDebounceTime[i]) > DEBOUNCE_MS) {
      // Has the stableState actually changed?
      if (stableState[i] != rawReading) {
        stableState[i] = rawReading;

        // -------------------------
        // Handle stable transitions
        // -------------------------

        // If stable state is now LOW => the switch is pressed
        if (stableState[i] == LOW && !isCurrentlyPressed[i]) {
          isCurrentlyPressed[i] = true;
          pressTime[i] = millis(); // Record when we started pressing
          Serial.print("Switch ");
          Serial.print(i);
          Serial.println(" pressed (stable).");
        } 
        
        // If stable state is now HIGH => the switch is released
        else if (stableState[i] == HIGH && isCurrentlyPressed[i]) {
          isCurrentlyPressed[i] = false;
          unsigned long duration = millis() - pressTime[i];

          // Decide short vs long press based on duration
          if (duration < LONG_PRESS_THRESHOLD) {
            Serial.print("Switch ");
            Serial.print(i);
            Serial.println(" short press action: ");
            Serial.println(normalKeys[i]);
            // Example action: sendShortPressKey(i) or similar
            sendKeyCombo(normalKeys[i], 0);
          } else {
            Serial.print("Switch ");
            Serial.print(i);
            Serial.print(" long press action: ");
            Serial.println(longPressKeys[i]);
            // Example action: sendLongPressKey(i) or similar
            sendKeyCombo(longPressKeys[i], 0);
          }
        }
      }
    }
  }

  // testKeyboard(HID_KEY_4, 0, "HID_KEY_4 - normal");
  // testKeyboard(HID_KEY_BACKSLASH, 0, "HID_KEY_BACKSLASH - normal");
  // testKeyboard(HID_KEY_PERIOD, 0, "HID_KEY_PERIOD - normal");
  // testKeyboard(HID_KEY_SEMICOLON, 0, "HID_KEY_SEMICOLON - normal");
  // testKeyboard(HID_KEY_SLASH, 0, "HID_KEY_SLASH - normal");
  // testKeyboard(HID_KEY_GRAVE, 0, "HID_KEY_GRAVE - normal");
  // testKeyboard(HID_KEY_COMMA, 0, "HID_KEY_COMMA - normal");
}
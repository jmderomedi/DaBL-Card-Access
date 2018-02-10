#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <TimerOne.h>

const uint8_t pn532_SCK  = 2;
const uint8_t pn532_MOSI = 3;
const uint8_t pn532_CS   = 4;
const uint8_t pn532_MISO = 5;

const uint8_t led_pins[] = {14, 15, 16};
bool common_anode = true;
uint8_t led_high, led_low;
String input_string = "";
bool stringComplete = false;
bool py_are_we_connected = false;
bool py_should_we_test_connection = false;
uint32_t py_test_connection_interval = 10000;
uint32_t timer;

Adafruit_PN532 nfc(pn532_SCK, pn532_MISO, pn532_MOSI, pn532_CS);

enum colors {red = 0, green, blue, yellow, cyan, magenta};

void setup() {
    if (common_anode) {
        led_high = 0;
        led_low = 255;
    } else {
        led_high = 255;
        led_low = 0;
    }
    for (byte i = 0 ; i < sizeof(led_pins)/sizeof(led_pins[0]) ; i++ ) {
        pinMode(led_pins[i], OUTPUT);
        analogWrite(led_pins[i], led_low);
    }
    nfc.begin();
    uint32_t version_data = nfc.getFirmwareVersion();
    if (! version_data) { // gotta reset hardware
        analogWrite(led_pins[red], led_high);
        while (1);
    } else {
        //blink_led(cyan, 3);
    }
    Serial.begin(115200);
    while(!Serial);
    nfc.setPassiveActivationRetries(0xFF); // set num of retries before fail (do i need?)
    nfc.SAMConfig(); // you gonna read some RFID cards
    py_test_connection();

}

void loop() {
    if (millis()-timer > py_test_connection_interval) { // see if connected to python
        py_test_connection();
    }
    if (py_are_we_connected) {
        bool success;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };    // Buffer to store the returned UID
        uint8_t uidLength;              // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
        // success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
        if (success) {
            // TO ADD: do some things here. for eample:
            // for (uint8_t i=0; i < uidLength; i++) {
            //     Serial.print(" 0x");Serial.print(uid[i], HEX); 
        }
        if (stringComplete) {
            if (input_string == "RESULT: user approved") {              // green
                blink_led(green, 3);
            } else if (input_string == "RESULT: user rejected") {       // red
                blink_led(red, 3);
            } else if (input_string == "RESULT: user not found") {      // yellow
                blink_led(yellow, 2);
            } else if (input_string == "RESULT: db not accessible") {   // blue
                blink_led(blue, 2);
            }
            // TO ADD: what did i get back from the server?:
                // [1] in db, approved:         green
                // [2] in db, NOT approved:     red
                // [3] NOT in db:               yellow
                // [4] cannot connect to db     blue

            input_string = "";
            stringComplete = false;
        }
    }
}

void serialEvent() {
    while (Serial.available()) {
        char inChar = (char)Serial.read();
        input_string += inChar;
        if (inChar == '\n') {
            stringComplete = true;
        }
    }
    Serial.println(input_string);
}

// TODO: needs to set a var that tells main loop to write card taps to eeprom/SD for later dump
void py_test_connection() {
    py_are_we_connected = false;
    uint32_t conn_timer = millis();
    Serial.println("Are you there, python?");
    analogWrite(led_pins[red], led_low);
    delay(100);
    while ((!py_are_we_connected) && (millis() - conn_timer < 1000)) {
        while (Serial.available()) {
            char inChar = (char)Serial.read();
            input_string += inChar;
            if (inChar == '\n') {
                stringComplete = true;
            }
        }
        if (stringComplete) {
            Serial.println("poop");
            String t = String(input_string);
            t.replace("\n", "");
            if (t == "I am here, teensy!") {
                input_string = "";
                stringComplete = false;
                py_are_we_connected = true;
                blink_led(blue, 2);
            }
        }
    }
    if (!py_are_we_connected) {
        analogWrite(led_pins[red], led_high);
    }
    py_should_we_test_connection = false;
    timer = millis();
}

void INT_py_test_connection() {
    py_should_we_test_connection = true;
}

// this may not work for magenta
// THIS IS PURPOSELY BLOCKING
void blink_led(uint8_t c, uint8_t n) {
    uint8_t c2 = c%3+1;
    if (c > 2) c = c%3;
    else c2 = 0;
    for (uint8_t i = 0 ; i > sizeof(led_pins)/sizeof(led_pins[0]) ; i++) {
        analogWrite(led_pins[i], led_low);
    }
    for (uint8_t i = 0 ; i < n ; i++) { // blink n times
        analogWrite(led_pins[c], led_high);
        analogWrite(led_pins[c2], led_high);
        delay(500);
        analogWrite(led_pins[c], led_low);
        analogWrite(led_pins[c2], led_low);
        delay(500);
    }
}


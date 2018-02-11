#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
// #include <TimerOne.h>

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
uint32_t py_test_connection_interval = 60000;
uint32_t ttt;
bool debug_mode = false;

Adafruit_PN532 nfc(pn532_SCK, pn532_MISO, pn532_MOSI, pn532_CS);

enum colors {red = 0, green, blue, yellow, cyan, magenta};

/* colors:
    HW      cyan        blink   connected to PN532
    HW      red         solid   NOT connected to PN532
    SW      magenta     blink   NOT connected to db
    SW      green       blink   in db, approved
    SW      red         blink   in db, NOT approved
    SW      yellow      blink   NOT in db
    SW      blue        blink   connected to python
    HW      red         solid   not connected to python

*/


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
        solid_led(red);
        while (1);
    } else {
        if (debug_mode) {
            blink_led(red, 2);
            blink_led(green, 2);
            blink_led(blue, 2);
            blink_led(yellow, 2);
            blink_led(cyan, 2);
            blink_led(magenta, 2);
        }
    }
    Serial.begin(115200);
    while(!Serial);
    nfc.setPassiveActivationRetries(0xFF); // set num of retries before fail (do i need?)
    nfc.SAMConfig(); // you gonna read some RFID cards
    py_test_connection();

}

void loop() {
    if (millis() - ttt > py_test_connection_interval) { // see if connected to python
        py_test_connection();
    }
    if (py_are_we_connected) {
        bool success;
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
        uint8_t uidLength;
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
        if (success) {
            print_hex(uid, uidLength);
        }
        if (stringComplete) {
            String t =  String(input_string);
            t.replace("\n", "");
            if (t == "RESULT: user approved") {              // green
                blink_led(green, 3);
            } else if (t == "RESULT: user rejected") {       // red
                blink_led(red, 3);
            } else if (t == "RESULT: user not found") {      // yellow
                blink_led(yellow, 3);
            } else if (t == "RESULT: db not accessible") {   // magenta
                blink_led(magenta, 3);
            }
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
}

// TODO: needs to set a var that tells main loop to write card taps to eeprom/SD for later dump
void py_test_connection() {
    py_are_we_connected = false;
    ttt = millis();
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
            String t = String(input_string);
            t.replace("\n", "");
            if (t == "I am here, teensy!") {
                input_string = "";
                stringComplete = false;
                py_are_we_connected = true;
                blink_led(blue, 1);
            }
        }
    }
    if (!py_are_we_connected) {
        analogWrite(led_pins[red], led_high);
    }
}

// this may not work for magenta
// THIS IS PURPOSELY BLOCKING
void blink_led(uint8_t c) {
    uint8_t c1 = c%3;
    uint8_t c2 = c1+1;
    if (c2 > 2) c2 = 0;
    for (uint8_t i = 0 ; i > sizeof(led_pins)/sizeof(led_pins[0]) ; i++) {
        analogWrite(led_pins[i], led_low);
    }
    for (uint8_t i = 0 ; i < 2 ; i++) { // blink n times
        analogWrite(led_pins[c1], led_high);
        if (c > 2) analogWrite(led_pins[c2], led_high);
        delay(500);
        analogWrite(led_pins[c1], led_low);
        if (c > 2) analogWrite(led_pins[c2], led_low);
        delay(500);
    }
}

void blink_led(uint8_t c, uint8_t n) {
    uint8_t c1 = c%3;
    uint8_t c2 = c1+1;
    if (c2 > 2) c2 = 0;
    for (uint8_t i = 0 ; i > sizeof(led_pins)/sizeof(led_pins[0]) ; i++) {
        analogWrite(led_pins[i], led_low);
    }
    for (uint8_t i = 0 ; i < n ; i++) { // blink n times
        analogWrite(led_pins[c1], led_high);
        if (c > 2) analogWrite(led_pins[c2], led_high);
        delay(300);
        analogWrite(led_pins[c1], led_low);
        if (c > 2) analogWrite(led_pins[c2], led_low);
        delay(300);
    }
}

void solid_led(uint8_t c) {
    analogWrite(led_pins[c], led_high);
}

void print_hex(const byte * data, const uint32_t numBytes) {
    uint32_t szPos;
    for (szPos=0; szPos < numBytes; szPos++) {
        // Append leading 0 for small values
        if (data[szPos] <= 0xF)
            Serial.print(F("0"));
        Serial.print(data[szPos]&0xff, HEX);
        if ((numBytes > 1) && (szPos != numBytes - 1)) {
            Serial.print(F(" "));
        }
    }
    Serial.println();
}


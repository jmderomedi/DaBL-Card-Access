#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <elapsedMillis.h>
#include <Keyboard.h>

const uint8_t pn532_SCK  = 2;
const uint8_t pn532_MOSI = 3;
const uint8_t pn532_CS   = 4;
const uint8_t pn532_MISO = 5;
char Enter = 0xB0;

const uint8_t led_pins[] = {14, 15, 16};
bool common_anode = true;
uint8_t led_high, led_low;
String input_string = "";
bool stringComplete = false;
bool py_are_we_connected = false;
bool py_should_we_test_connection = false;
uint32_t py_test_connection_interval = 60000;
uint32_t ttt;
elapsedMillis swipe_wait = 0;
bool debug_mode = false;
String windows_password = "password";

Adafruit_PN532 nfc(pn532_SCK, pn532_MISO, pn532_MOSI, pn532_CS);

enum colors {red = 0, green, blue, yellow, cyan, magenta};

#include <ESP8266WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>

char wifi_ssid[] = "ssid";                 // Network Name
char wifi_password[] = "password";                 // Network Password
byte mac[6];

WiFiServer server(80);
WiFiClient client;
MySQL_Connection conn((Client *)&client);

char SELECT_ACCESS[] = "SELECT name,%s FROM dabl.users WHERE uid=%s";
char query[128];
char msg[128];

IPAddress server_addr(192, 168, 0, 100);    // MySQL server IP
char mysql_user[] = "username";                 // MySQL user
char mysql_password[] = "password";                 // MySQL password

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
    Serial.begin(115200);
    while(!Serial);
    setup_pins();
    nfc.begin();
    uint32_t version_data = nfc.getFirmwareVersion();
    nfc.setPassiveActivationRetries(0xFF); // set num of retries before fail (do i need?)
    nfc.SAMConfig(); // you gonna read some RFID cards
    Serial.print("Connecting to ");
    Serial.println(wifi_ssid);
    WiFi.begin(wifi_ssid, wifi_password);
    elapsedMillis wifi_timer = 0;
    while ((WiFi.status() != WL_CONNECTED) && (wifi_timer<8000)) {
        delay(200);
        Serial.print(".");
    }
    if (wifi_timer >= 8000) {
        Serial.println("WiFi NOT CONNECTED. Please check your connections/settings.");
        while(1);
    }
    Serial.println("WiFi connected.");
    WiFi.macAddress(mac);
    Keyboard.begin();
    print_mac();
}

void loop() {
    // if (swipe_wait > 3000) {
    //     bool success;
    //     uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
    //     uint8_t uidLength;
    //     success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
    //     if (success) {
    //         print_hex(uid, uidLength);
    //         swipe_wait = 0;
    //     }
    // }
    // if (stringComplete) {
    //     if (readid == card1) {
    //         sign_in_out();
    //     }
    //     String t =  String(input_string);
    //     t.replace("\n", "");
    //     if (t == "RESULT: user approved") {              // green
    //         blink_led(green, 3);
    //     } else if (t == "RESULT: user rejected") {       // red
    //         blink_led(red, 3);
    //     } else if (t == "RESULT: user not found") {      // yellow
    //         blink_led(yellow, 3);
    //     } else if (t == "RESULT: db not accessible") {   // magenta
    //         blink_led(magenta, 3);
    //     }
    //     input_string = "";
    //     stringComplete = false;
    // }
    bool are_we_connected = connect_to_db();
    if (are_we_connected) {
        //select_all();
        //bool access_level = access_check("\'A4 7E 4C 12\'");
        bool waiver = check_access("\'A4 7E 4C 12\'", "waiver");
        Serial.print("waiver: ");
        Serial.println(waiver);
        bool bantam = check_access("\'7A 6B 8B D1\'", "bantamtools");
        Serial.print("bantamtools: ");
        Serial.println(bantam);
        if (bantam) {
            // do a thing
        }
        conn.close();
    } else {
        
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

// this may not work for magenta
// THIS IS PURPOSELY BLOCKING
void blink_led(uint8_t c) {
    uint8_t n = 2;
    blink_led(c, n);
}

// overloaded
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

void sign_in_out() {
    Keyboard.press(KEY_LEFT_GUI);              //Press the left windows key.
    Keyboard.press('l');                       //Press the "l" key.
    Keyboard.releaseAll();                     //Release all keys.
    delay (100);
    Keyboard.press(Enter);                     //Press the Enter key.
    Keyboard.release(Enter);                   //Release the Enter key.
    delay(100);
    Keyboard.print(windows_password);                // Change this value to your Windows PIN/Password.
    Keyboard.releaseAll();
    delay(100);
    Keyboard.press(Enter);
    Keyboard.releaseAll();
}

void setup_pins() {
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
    if (! version_data) { //  reset hardware
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
}

void print_mac() {
    Serial.print("MAC:\t\t\t");
    Serial.print(mac[5], HEX);
    Serial.print(":");
    Serial.print(mac[4], HEX);
    Serial.print(":");
    Serial.print(mac[3], HEX);
    Serial.print(":");
    Serial.print(mac[2], HEX);
    Serial.print(":");
    Serial.print(mac[1], HEX);
    Serial.print(":");
    Serial.println(mac[0], HEX);
    Serial.print("Assigned IP:\t");
    Serial.print(WiFi.localIP());
    Serial.println("");
}

bool check_access(String uid, String machine) {
    sprintf(query, SELECT_ACCESS, machine.c_str(), uid.c_str());
    Serial.print("Query: ");
    Serial.println(query);
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    // Execute the query
    cur_mem->execute(query);
    // Fetch the columns and print them
    Serial.print("-- ");
    column_names *cols = cur_mem->get_columns();
    for (int f = 0; f < cols->num_fields; f++) {
        Serial.print(cols->fields[f]->name);
        if (f < cols->num_fields-1) {
            Serial.print("\t\t\t");
        }
    }
    Serial.print("\n-- ");
    // Read the rows and print them
    row_values *row = NULL;
    String cell;
    do {
        row = cur_mem->get_next_row();
        if (row != NULL) {
            for (int f = 0; f < cols->num_fields; f++) {
                cell = row->values[f];
                Serial.print(cell);
                if (f < cols->num_fields-1) {
                    Serial.print("\t\t");
                }
            }
            Serial.println();
        }
    } while (row != NULL);
    // Deleting the cursor also frees up memory used
    delete cur_mem;
    if (atoi(cell.c_str()) == 1) {
        return true;
    } else return false;
}

bool connect_to_db() {
    uint8_t attempts = 0;
    Serial.println("Connecting to database");
    while (conn.connect(server_addr, 3306, mysql_user, mysql_password) != true && attempts < 10) {
        delay(500);
        Serial.print (".");
        attempts++;
    }
    Serial.println("");
    if (attempts != 10) {
        Serial.println("-- Connected to SQL Server!");
        return true;
    } else {
        Serial.println("-- UNABLE TO CONNECT to SQL server.");
        return false;
    }
}


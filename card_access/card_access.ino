#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <elapsedMillis.h>
#include <WiFiEsp.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
//#include <TimeLib.h> 
#include <WiFiEspUdp.h>

#define DEBUG 1

// pn532
const uint8_t pn532_SCK  = 13;
const uint8_t pn532_MOSI = 11;
const uint8_t pn532_CS   = 10;
const uint8_t pn532_MISO = 12;
char Enter = 0xB0;
Adafruit_PN532 nfc(pn532_SCK, pn532_MISO, pn532_MOSI, pn532_CS);
bool rfid_received = false;
elapsedMillis swipe_time_buffer = 0;
String uid = "";
String windows_password = "";    // Windows password

// esp8266
WiFiEspClient client;
byte esp8266_mac[6];
char wifi_WAN_ssid[] = "";                  // Network Name
char wifi_WAN_password[] = "";       // Network Password
char wifi_LAN_ssid[] = "";                  // Network Name
char wifi_LAN_password[] = "";       // Network Password

// server
IPAddress server_addr(192, 168, 0, 100);    // MySQL server IP
MySQL_Connection conn((Client *)&client);
char mysql_user[] = "root";                 // MySQL user
char mysql_password[] = "x";                // MySQL password
// possibilities: master, typea, stratasys, bantam, pls475, jaguarv
char SELECT_ACCESS[] = "SELECT name,uid,%s FROM %s WHERE uid=\'%s\'";
char UPDATE_USER[] = "UPDATE %s SET `lastaccess`=CURRENT_TIMESTAMP() WHERE uid=\'%s\'";
char INSERT_ACCESS[] = "INSERT INTO %s (name, uid) VALUES (\'%s\', \'%s\')";
char msg[128];
String machine = "waiver";

// general hardware
// esp8266: 0/1 (TX/RX), 4 (reset)
uint8_t led_pins[] = {3, 4, 5};
bool common_anode = false;
uint8_t led_low, led_high;
enum colors {red = 0, green, blue, yellow, cyan, magenta};

/* colors:
    red         solid   NOT connected to PN532
    yellow      solid   -
    green       solid   -
    cyan        solid   NOT connected to wifi
    blue        solid   -
    magneta     solid   -
    red         blink   in db, NOT approved
    yellow      blink   couldn't connect to DB
    green       blink   in db, approved
    cyan        blink   -
    blue        blink   -
    magenta     blink   -
*/

void setup() {
    Serial.begin(115200);
    //while (!Serial);
    Serial1.begin(9600);
    WiFi.init(&Serial1);
    setup_pins();
    for (int i = 0 ; i < 6 ; i++) {
        blink_led(i, 1);
    }
    nfc.begin();
    delay(100);
    uint32_t version_data = nfc.getFirmwareVersion(); // do i need this?
    if (!version_data) {
        Serial.println("Not connected to PN532.");
        turn_off_leds();
        solid_led(red);
    }
    nfc.setPassiveActivationRetries(0xFF); // set num of retries before fail (do i need this either?)
    nfc.SAMConfig(); // you gonna read some RFID cards
    wifi_start(wifi_LAN_ssid, wifi_LAN_password);
    //setSyncProvider(getTeensy3Time);
    delay(100);
//    if (timeStatus()!= timeSet) {
//        Serial.println("Unable to sync with the RTC");
//    } else {
//        Serial.println("RTC has set the system time");
//    }
}

void loop() {
    if (swipe_time_buffer > 3000) {
        bool success;
        uint8_t uid_raw[] = { 0, 0, 0, 0, 0, 0, 0 };
        uint8_t uidLength;
        rfid_received = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid_raw, &uidLength);
        if (rfid_received) {
            uid = print_hex(uid_raw, uidLength);
            swipe_time_buffer = 0;
        }
    }
    if (rfid_received) {
        rfid_received = false;
        sprintf(msg, "Checking DB for %s...", uid.c_str());
        Serial.println(msg);
        if (WiFi.status() == WL_CONNECTED) {
            bool are_we_connected = connect_to_db();
            if (are_we_connected) {
                String nname = check_access(F("dabl.users"), uid, machine);
                Serial.print("access: ");
                Serial.println(nname);
                if (nname != "NULL") {
                    Serial.println("you're in!");
                    if (machine == "waiver") {
                        // dbnametable, uid, name, date, time
                        //char date_str[128];
                        //char time_str[128];
                        //sprintf(date_str, "%04d/%02d/%02d", year(), month(), day());
                        //sprintf(time_str, "%02d:%02d:%02d", hour(), minute(), second());
                        bool success = log_access(uid, nname);
                        if (success) {
//                            Serial.println("-- Successfully added entry to database");
                        } else {
//                            Serial.println("-- Unable to add entry to database");
                        }
                    } else {
                        Serial.println("-- Access granted. signing in/out.");
                        sign_in_out();
                    }
                    blink_led(green, 4);
                } else {
                    Serial.println("-- Oh hell nah.");
                    blink_led(red, 4);
                }
                conn.close();
            } else {
                Serial.println("-- Could not connect to database.");
                blink_led(yellow, 4);
            }
        } else {
            Serial.println("Not connected to wifi. Womp.");
            turn_off_leds();
            solid_led(cyan);
        }
    }
}

bool wifi_start(char *ssid, char *pass) {
    turn_off_leds();
    WiFi.disconnect();
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    elapsedMillis wifi_timer = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_timer<10000);
//    if (wifi_timer >= 10000) {
//        Serial.println("-- WiFi NOT CONNECTED. Please check your connections/settings.");
//        solid_led(cyan);
//        return false;
//    }
    WiFi.macAddress(esp8266_mac);
    print_network();
    return true;
}

void print_network() {
    Serial.print("-- MAC:\t\t");
    for (int i = 5 ; i >= 0 ; i--) {
        Serial.print(esp8266_mac[i], HEX);
        if (i != 0) Serial.print(":");
        else Serial.println();
    }
    Serial.print("-- IP:\t\t");
    Serial.println(WiFi.localIP());
}

// todo: if dbquery returns anything other than 1 user, return false
// todo: hangs if given malformed sql request
// todo: fix header column printing to use tabs
String check_access(String dbnametable, String uid, String machine) {
    char query[128];
    String cell;
    // machine, dbnametable, uid
    sprintf(query, SELECT_ACCESS, machine.c_str(), dbnametable.c_str(), uid.c_str());
    Serial.print("Query: ");
    Serial.println(query);
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    // Execute the query
    cur_mem->execute(query);
    // Fetch the columns and print them
    Serial.print("-- ");
    column_names *cols = cur_mem->get_columns();
    for (int f = 0; f < cols->num_fields; f++) {
        cell = cols->fields[f]->name;
        cell.toCharArray(query, 128);
        sprintf(msg, "%-30s",query);
        Serial.print(msg);
        if (f < cols->num_fields-1) {
            //Serial.print(".");
        }
    }
    
    Serial.print("\n-- ");
    // Read the rows and print them
    row_values *row = NULL;
    String nname;
    do {
        row = cur_mem->get_next_row();
        if (row != NULL) {
            for (int f = 0; f < cols->num_fields; f++) {
                cell = row->values[f];
                cell.toCharArray(query, 128);
                if (f == 0) nname = String(cell);
                sprintf(msg, "%-30s",query);
                Serial.print(msg);
            }
            Serial.print("\n-- ");
        }
    } while (row != NULL);
    Serial.println();
    // Deleting the cursor also frees up memory used
    delete cur_mem;
    if (atoi(cell.c_str()) == 1) {
        return nname;
    } else return "NULL";
}

bool connect_to_db() {
    elapsedMillis timeout = 0;
    //Serial.println("Connecting to database");
    while (conn.connect(server_addr, 3306, mysql_user, mysql_password) != true && timeout < 10000) {
        delay(500);
        Serial.print (".");
    }
    Serial.println("");
    if (timeout < 10000) {
        Serial.println("-- Connected to SQL Server!");
        return true;
    } else {
        Serial.println("-- UNABLE TO CONNECT to SQL server.");
        return false;
    }
}

// todo: better returning of information
bool log_access(String uid, String nname) {
    char query[128];

    String dbnametable = "dabl.users";
    // dbnametable, uid
    sprintf(query, UPDATE_USER, dbnametable.c_str(), uid.c_str());
    Serial.print("Query: ");
    Serial.println(query);
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    // Execute the query
    cur_mem->execute(query);

    dbnametable = "dabl.accesslog";
    // dbnametable, name, uid
    sprintf(query, INSERT_ACCESS, dbnametable.c_str(), nname.c_str(), uid.c_str());
    Serial.print("Query: ");
    Serial.println(query);
    // Execute the query
    cur_mem->execute(query);
    delete cur_mem;
    return true;
}

void sign_in_out() {
//    Keyboard.press(KEY_LEFT_GUI);              //Press the left windows key.
//    Keyboard.press('l');                       //Press the "l" key.
//    Keyboard.releaseAll();                     //Release all keys.
//    delay (100);
//    Keyboard.press(Enter);                     //Press the Enter key.
//    Keyboard.release(Enter);                   //Release the Enter key.
//    delay(100);
//    Keyboard.print(windows_password);
//    Keyboard.releaseAll();
//    delay(100);
//    Keyboard.press(Enter);
//    Keyboard.releaseAll();
}

String print_hex(const byte * data, const uint32_t numBytes) {
    uint32_t szPos;
    String str = "";
    for (szPos=0; szPos < numBytes; szPos++) {
        // Append leading 0 for small values
        if (data[szPos] <= 0xF) {
            //Serial.print(F("0"));
            str += F("0");
        }
        //Serial.print(data[szPos]&0xff, HEX);
        str += String(data[szPos]&0xff, HEX);
        if ((numBytes > 1) && (szPos != numBytes - 1)) {
            //Serial.print(F(" "));
            str += F(" ");
        }
    }
    //Serial.println();
    str.toUpperCase();
    return str;
}

// ====================================================================================
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
}

bool turn_off_leds() {
    for (byte i = 0 ; i < sizeof(led_pins)/sizeof(led_pins[0]) ; i++ ) {
        analogWrite(led_pins[i], led_low);
    }
    return true;
}

time_t getTeensy3Time() {
    return Teensy3Clock.get();
}


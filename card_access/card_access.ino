#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <elapsedMillis.h>
#include <WiFiEsp.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <TimeLib.h> 
#include <WiFiEspUdp.h>

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
char wifi_ssid[] = "AUGuest-ByRCN";                  // Network Name
char wifi_password[] = "";       // Network Password

// server
WiFiEspServer server(80);
IPAddress server_addr(192, 168, 0, 101);    // MySQL server IP
MySQL_Connection conn((Client *)&client);
char mysql_user[] = "";                 // MySQL user
char mysql_password[] = "";                // MySQL password

// time stuff
char timeServer[] = "time.nist.gov";  // NTP server
unsigned int localPort = 2390;        // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;  // NTP timestamp is in the first 48 bytes of the message
const int UDP_TIMEOUT = 2000;    // timeout in miliseconds to wait for an UDP packet to arrive
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
WiFiEspUDP Udp;

// search and general
// possibilities: master, typea, stratasys, bantam, pls475, jaguarv
char SELECT_ACCESS[] = "SELECT name,uid,%s FROM %s WHERE uid=\'%s\'";
char UPDATE_USER[] = "UPDATE %s SET lastaccessdate=\'%s\', lastaccesstime=\'%s\' WHERE uid=\'%s\'";
char INSERT_ACCESS[] = "INSERT INTO %s (name, uid, accessdate, accesstime) VALUES (\'%s\', \'%s\', \'%s\', \'%s\')";
char msg[128];
String machine = "waiver";

void setup() {
    nfc.begin();
    uint32_t version_data = nfc.getFirmwareVersion(); // do i need this?
    nfc.setPassiveActivationRetries(0xFF); // set num of retries before fail (do i need this either?)
    nfc.SAMConfig(); // you gonna read some RFID cards
    Serial.begin(115200);
    while (!Serial);
    Serial1.begin(115200);
    WiFi.init(&Serial1);
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(wifi_ssid);
    WiFi.begin(wifi_ssid, wifi_password);
    elapsedMillis wifi_timer = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_timer<10000);
    if (wifi_timer >= 10000) {
        Serial.println("WiFi NOT CONNECTED. Please check your connections/settings.");
        while(1);
    }
    WiFi.macAddress(esp8266_mac);
    print_network();
    Udp.begin(localPort);
    while (1) {
        if (WiFi.status() == WL_CONNECTED) {
            get_time();
            delay(10000);
        }
    }
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
        bool are_we_connected = connect_to_db();
        if (are_we_connected) {
            bool access = check_access(F("dabl.users"), uid, machine);
            Serial.print("access: ");
            Serial.println(access);
            if (access) {
                Serial.println("you're in!");
                if (machine == "waiver") {
                    // dbnametable, uid, name, date, time
                    bool success = log_access("", uid, "McDougal, DingDong", "today", "right now");
                    if (success) {
                        Serial.println("-- Successfully added entry to database");
                    } else {
                        Serial.println("-- Unable to add entry to database");
                    }
                } else {
                    Serial.println("-- Access granted. signing in/out.");
                    //sign_in_out();
                }
            } else {
                Serial.println("-- Oh hell nah.");
            }
            conn.close();
        } else {
            Serial.println("Not connected to DB. Check your settings.");
        }
    }
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
bool check_access(String dbnametable, String uid, String machine) {
    char query[128];
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
        Serial.print(cols->fields[f]->name);
        if (f < cols->num_fields-1) {
            //Serial.print(".");
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
                cell.toCharArray(query, 128);
                sprintf(msg, "%-20s",query);
                Serial.print(msg);
            }
            Serial.print("\n-- ");
        }
    } while (row != NULL);
    Serial.println();
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

// todo: better returning of information
bool log_access(String dbnametable, String uid, String name, String date, String time) {
    char query[128];

    dbnametable = "dabl.users";
    // dbnametable, date, time, uid
    sprintf(query, UPDATE_USER, dbnametable.c_str(), date.c_str(), time.c_str(), uid.c_str());
    Serial.print("Query: ");
    Serial.println(query);
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    // Execute the query
    cur_mem->execute(query);

    dbnametable = "dabl.accesslogs";
    // dbnametable, name, uid, date, time
    sprintf(query, INSERT_ACCESS, dbnametable.c_str(), name.c_str(), uid.c_str(), date.c_str(), time.c_str());
    Serial.print("Query: ");
    Serial.println(query);
    // Execute the query
    cur_mem->execute(query);
    delete cur_mem;



    return false;
}

void sign_in_out() {
    Keyboard.press(KEY_LEFT_GUI);              //Press the left windows key.
    Keyboard.press('l');                       //Press the "l" key.
    Keyboard.releaseAll();                     //Release all keys.
    delay (100);
    Keyboard.press(Enter);                     //Press the Enter key.
    Keyboard.release(Enter);                   //Release the Enter key.
    delay(100);
    Keyboard.print(windows_password);
    Keyboard.releaseAll();
    delay(100);
    Keyboard.press(Enter);
    Keyboard.releaseAll();
}

String print_hex(const byte * data, const uint32_t numBytes) {
    uint32_t szPos;
    String str = "";
    for (szPos=0; szPos < numBytes; szPos++) {
        // Append leading 0 for small values
        if (data[szPos] <= 0xF) {
            Serial.print(F("0"));
            str += F("0");
        }
        Serial.print(data[szPos]&0xff, HEX);
        str += String(data[szPos]&0xff, HEX);
        if ((numBytes > 1) && (szPos != numBytes - 1)) {
            Serial.print(F(" "));
            str += F(" ");
        }
    }
    Serial.println();
    str.toUpperCase();
    return str;
}

void get_time() {
    sendNTPpacket(timeServer); // send an NTP packet to a time server
  
  // wait for a reply for UDP_TIMEOUT miliseconds
  unsigned long startMs = millis();
  while (!Udp.available() && (millis() - startMs) < UDP_TIMEOUT) {}

  Serial.println(Udp.parsePacket());
  if (Udp.parsePacket()) {
    Serial.println("packet received");
    // We've received a packet, read the data from it into the buffer
    Udp.read(packetBuffer, NTP_PACKET_SIZE);

    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = ");
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);


    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if (((epoch % 3600) / 60) < 10) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ((epoch % 60) < 10) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second
  }
  // wait ten seconds before asking for the time again
}

// send an NTP request to the time server at the given address
void sendNTPpacket(char *ntpSrv)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)

  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(ntpSrv, 123); //NTP requests are to port 123

  Udp.write(packetBuffer, NTP_PACKET_SIZE);

  Udp.endPacket();
}

// ====================================================================================
// possibly deprecated functions follow
/*
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
*/

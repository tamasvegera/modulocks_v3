#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include "MFRC522_.h"
#include <Wire.h>
#include "clsPCA9555.h"
#include <SPI.h>
#include <Ticker.h>

const char* fw_version = "1.0";

#define NEEDED_EEPROM_SIZE    2048
#define CONFIG_EEPROM_ADDR    0
#define MAX_SENSORS   10
#define NO_SENSOR_TYPES 5   // add 1 extra to sensor types
#define NO_RELAY_OUTS   3
#define MAX_SOUND_FILENAME_SIZE 64
#define MAX_CARD_LABEL_LENGTH 20

#define STORED_RFID_CARDS   5
#define CARD_UID_MAX_LENGTH   10

#define SENSOR_VDD_EN_PIN_IO_EXP  0
#define LED_BLUE_PIN  4
#define LED_RED_PIN 5

PCA9555 io_exp(0x20);
Ticker sec_ticker;

enum game_status{NOT_SOLVED, DELAYED_SOLVED, WAIT_FOR_CHECK, SOLVED};

enum relay_states{RELAY_OFF, RELAY_DELAY_ON_STARTED, RELAY_DELAY_ON_EXPIRED, RELAY_DELAY_OFF_STARTED, RELAY_DELAY_OFF_EXPIRED};

enum game_status game_is_solved = NOT_SOLVED;
uint8_t game_solved_counter = 0;

String sensor_types[NO_SENSOR_TYPES] = {"None", "RFID", "IR", "Touch", "Hall"};
enum sensor_types_enum{SENSOR_NONE, SENSOR_RFID, SENSOR_IR, SENSOR_TOUCH, SENSOR_HALL};

//SSID and Password to your ESP Access Point
const char* ssid = "ESPWebServer";
const char* password = "12345678";

uint8_t reconfig_is_needed = false;

uint8_t io_to_expio[MAX_SENSORS] = {17, 16, 15, 14, 13, 12, 10, 11, 6, 7};
uint8_t relay_expios[NO_RELAY_OUTS] = {1, 2, 3};

uint8_t heartbeat=0;

MFRC522 *rfids[MAX_SENSORS];

ESP8266WebServer server(80); //Server on port 80
ESP8266HTTPUpdateServer httpUpdater;

enum relay_switch_states {relay_NC, relay_NO};

struct RFID_card
{
  char card_name[MAX_CARD_LABEL_LENGTH];
  uint8_t uid[CARD_UID_MAX_LENGTH];
  uint8_t uid_length;
};
struct RFID_sensor_config
{
  struct RFID_card cards[STORED_RFID_CARDS];
};
struct SensorConfig
{
  uint8_t type, on_off;
};

struct RelayConfig
{
  enum relay_switch_states nc_no;
  uint8_t on_delay, on_delay_counter;
  uint8_t off_delay, off_delay_counter;
  enum relay_states state;
};

typedef struct CONFIG_s
{
    struct SensorConfig all_sensor_config[MAX_SENSORS];
    struct RelayConfig relays_config[NO_RELAY_OUTS];
    char sound_file[MAX_SOUND_FILENAME_SIZE];
    uint8_t game_solved_delay;
    struct RFID_sensor_config rfid_sensor[MAX_SENSORS];
} main_config_type;
volatile main_config_type main_config;

struct RFID_card last_read_cards[MAX_SENSORS];

String char_array_to_String(char* data)
{
  String new_string="";
  uint8_t i;

  for(i=0; data[i]!='\0'; i++)
    new_string += data[i];

  return new_string;
}

// max string length: 256
void String_to_volatile_char_array(volatile char* data, String new_string, uint8_t string_length)
{
  char char_array[256] = "";
  uint8_t i;

  new_string.toCharArray(char_array, string_length);
  memcpy((char*)data, char_array, string_length);  
}

String SendMainHTML()
{
  volatile char test_char_array[] = "asdf";
  uint8_t i, j;
  
  String ptr = "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\"></head>";
  ptr+= "<center><b>ModuLocks controller configuration</center></b><br>";

  ptr += "<a href=\"/rfid_config\">";
  ptr += "<button>Config RFID cards</button></a><br><br>";
  
  ptr += "<form action=\"/config\" method=\"POST\">";

  for(i=0; i<MAX_SENSORS; i++)
  {
    String sensor_number = String(i), sensor_number_pp = String(i+1);
    ptr += "Sensor in " + sensor_number_pp + ": ";
    ptr += "<select name=\"sensor_list" + sensor_number += "\">";
    for(j=0; j<NO_SENSOR_TYPES; j++)
    {
      ptr += "<option id=\"" + sensor_types[j] + "\"";;
      if(main_config.all_sensor_config[i].type == j)
        ptr += " selected"; 
      ptr += ">" + sensor_types[j] + "</option>";
    }
    ptr += "</select>";

    ptr += "<input type=\"radio\" name=\"on_off" + sensor_number + "\" value=\"on\"";
    if(main_config.all_sensor_config[i].on_off)
      ptr+= " checked";
    ptr += ">ON";
    
    ptr += "<input type=\"radio\" name=\"on_off" + sensor_number + "\" value=\"off\"";
    if(main_config.all_sensor_config[i].on_off == 0)
      ptr+= " checked";
    ptr += ">OFF";
    ptr += "<br>";
  }
  ptr += "<br>Delay before game is solved: ";
  ptr += "<input type=\"number\" name=\"game_solved_delay\" min=\"0\" max=\"10\" value=\"" + String(main_config.game_solved_delay) + "\"><br>";
  
  ptr += "<br>Set what should happen when game is solved:<br>";
  for(i=0; i<NO_RELAY_OUTS; i++)
  {    
    String relay_number = String(i), relay_number_pp = String(i+1);
    ptr += "Relay output " + relay_number_pp + ": ";
    ptr += "<input type=\"radio\" name=\"nc_no" + relay_number + "\" value=\"NC\"";
    if(main_config.relays_config[i].nc_no == relay_NC)
      ptr += " checked";
    ptr += ">NC";
    
    ptr += "<input type=\"radio\" name=\"nc_no" + relay_number + "\" value=\"NO\"";
    if(main_config.relays_config[i].nc_no == relay_NO)
      ptr += " checked";
    ptr += ">NO<br>";

    ptr += "    Delay before activate: ";
    ptr += "<input type=\"number\" name=\"on_delay" + relay_number + "\" min=\"0\" max=\"10\" value=\"" + String(main_config.relays_config[i].on_delay) + "\"><br>";
    
    ptr += "    Delay before deactivate: ";
    ptr += "<input type=\"number\" name=\"off_delay" + relay_number + "\" min=\"0\" max=\"10\" value=\"" + String(main_config.relays_config[i].off_delay) + "\"><br>";
  }
  ptr += "Sound file (max. " + String(MAX_SOUND_FILENAME_SIZE) + " characters: ";

  //ptr += "<input type=\"text\" name=\"sound\" value=\"" + String(main_config.sound_file) + "\"><br>";
  ptr += "<input type=\"text\" name=\"sound\" value=\"" + char_array_to_String((char*)main_config.sound_file) + "\"><br>";
  ptr += "<input type=\"submit\" value=\"Save\">";
  ptr += "</form><br>";

  ptr += "<a href=\"/update\">";
  ptr += "<button>Firmware update</button></a><br><br>";

  ptr += "Firmware version: ";
  ptr += fw_version;

  return ptr;
}

void reconfig()
{
  uint8_t i;
  for(i=0; i<MAX_SENSORS; i++)
  {
    delete rfids[i];
    switch(main_config.all_sensor_config[i].type)
    {
      case SENSOR_NONE:
        io_exp.pinMode(io_to_expio[i], INPUT);
        break;
      case SENSOR_RFID:
        rfids[i] = new MFRC522(&io_exp, io_to_expio[i], 0xFF);
        rfids[i]->PCD_Init();
        delay(100);
        rfids[i]->PCD_DumpVersionToSerial();
        delay(100);
        break;
      case SENSOR_IR:
        io_exp.pinMode(io_to_expio[i], INPUT);
        break;
      case SENSOR_TOUCH:
        io_exp.pinMode(io_to_expio[i], INPUT);
        break;
      case SENSOR_HALL:
        io_exp.pinMode(io_to_expio[i], INPUT);
        break;
    }
  }
}

//==============================================================
//     This rutine is exicuted when you open its IP in browser
//==============================================================
void handleRoot() {
  server.send(200, "text/html", SendMainHTML());
}

void handleConfig()
{
  uint8_t i,j;
  String sensor_list_number, on_off_number, nc_no_number, on_delay_number, off_delay_number;
  
  for(i=0; i<MAX_SENSORS; i++)
  {
    sensor_list_number = "sensor_list" + String(i);
    on_off_number = "on_off" + String(i);
    
    for(j=0; j<NO_SENSOR_TYPES; j++)
      if(server.arg(sensor_list_number) == sensor_types[j])
        main_config.all_sensor_config[i].type = j;

    if(server.arg(on_off_number) == "on")
      main_config.all_sensor_config[i].on_off = 1;
    else
      main_config.all_sensor_config[i].on_off = 0;
  }
  for(i=0; i<NO_RELAY_OUTS; i++)
  {
    main_config.relays_config[i].state = RELAY_OFF;
    main_config.relays_config[i].on_delay_counter = 0;
    main_config.relays_config[i].off_delay_counter = 0;
    
    nc_no_number = "nc_no" + String(i);
    if(server.arg(nc_no_number) == "NC")
      main_config.relays_config[i].nc_no = relay_NC;
    else
      main_config.relays_config[i].nc_no = relay_NO;    

    on_delay_number = "on_delay" + String(i);
    main_config.relays_config[i].on_delay = (server.arg(on_delay_number)).toInt();

    off_delay_number = "off_delay" + String(i);
    main_config.relays_config[i].off_delay = (server.arg(off_delay_number)).toInt();
  }
  main_config.game_solved_delay = (server.arg("game_solved_delay")).toInt();
  
  //server.arg("sound").toCharArray(main_config.sound_file, MAX_SOUND_FILENAME_SIZE);
  String_to_volatile_char_array(main_config.sound_file, server.arg("sound"), MAX_SOUND_FILENAME_SIZE);
  
  //Serial.println(server.arg("sound"));
  EEPROM.put(CONFIG_EEPROM_ADDR, main_config);
  EEPROM.commit();

  reconfig_is_needed = true;
  
/*  Serial.println("config handler");
  for(int i=0; i<server.args(); i++)
  {
    Serial.println(server.argName(i));
    Serial.println(server.arg(i));
  }
  */
  server.sendHeader("Location","/");
  server.send(303);  
}

void reset_config()
{
  uint8_t i,j,k;
  main_config.game_solved_delay = 0;
  
  for(i=0; i<MAX_SENSORS; i++)
  {
    main_config.all_sensor_config[i].type = 0;
    main_config.all_sensor_config[i].on_off = 0;
    for(j=0; j<STORED_RFID_CARDS; j++)
    {
      main_config.rfid_sensor[i].cards[j].uid_length = 0;
      for(k=0; k<MAX_CARD_LABEL_LENGTH; k++)
        main_config.rfid_sensor[i].cards[j].card_name[k] = 0;
    }
  }
  for(i=0; i<NO_RELAY_OUTS; i++)
  {
    main_config.relays_config[i].state = RELAY_OFF;
    main_config.relays_config[i].nc_no = relay_NO;
    main_config.relays_config[i].on_delay = 0;
    main_config.relays_config[i].off_delay = 0;
    main_config.relays_config[i].on_delay_counter = 0;
    main_config.relays_config[i].off_delay_counter = 0;
  }

  for(i=0; i<MAX_SOUND_FILENAME_SIZE; i++)
    main_config.sound_file[i] = 0;

  EEPROM.put(CONFIG_EEPROM_ADDR, main_config);
  EEPROM.commit();
  reconfig();
}

void print_config()
{
  uint8_t i;
  for(i=0; i<MAX_SENSORS; i++)
  {
    Serial.print("Sensor input ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(sensor_types[main_config.all_sensor_config[i].type]);
  }
  Serial.println((char*)main_config.sound_file);
}

void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

uint8_t check_game_solved()
{
  uint8_t game_solved = true, i, j, k, current_card_matched;
  for(i=0; i<MAX_SENSORS; i++)
  {
    switch(main_config.all_sensor_config[i].type)
    {
      case SENSOR_RFID:
        rfids[i]->PCD_Init();
        if(rfids[i]->PICC_IsNewCardPresent() != main_config.all_sensor_config[i].on_off)    // XOR
        {
          game_solved = false;
//          delay(50);
        }
        else if(rfids[i]->PICC_ReadCardSerial() != main_config.all_sensor_config[i].on_off)
        {
          game_solved = false;
          //delay(50);
        }
        if(game_solved)
        {
          // copy current read ID to last_read_cards
          for(j=0; j<rfids[i]->uid.size; j++)
            last_read_cards[i].uid[j] = rfids[i]->uid.uidByte[j];
          last_read_cards[i].uid_length = rfids[i]->uid.size;

          // check if card is in saved cards of that sensor
          game_solved = false;
          for(j=0; j<STORED_RFID_CARDS; j++)
          {
            current_card_matched = true;
            for(k=0; k< last_read_cards[i].uid_length; k++)
              if(main_config.rfid_sensor[i].cards[j].uid[k] != last_read_cards[i].uid[k])
                current_card_matched = false;
            if(current_card_matched && last_read_cards[i].uid_length)
            {
              game_solved = true;
              break;
            }
          }
        }
        else
        {
          last_read_cards[i].uid_length = 0;
          for(j=0; j<CARD_UID_MAX_LENGTH; j++)
            last_read_cards[i].uid[j] = 0x00;
        }   
        rfids[i]->PICC_IsNewCardPresent();
        break;
      case SENSOR_IR:
        if(io_exp.digitalRead(io_to_expio[i]) == main_config.all_sensor_config[i].on_off)
          game_solved = false;
        break;
      case SENSOR_HALL:
        if(io_exp.digitalRead(io_to_expio[i]) == main_config.all_sensor_config[i].on_off)
          game_solved = false;
        break;
      case SENSOR_TOUCH:
        break;
    }
  }
  return game_solved;
}

void ticker_handler(void)
{
  uint8_t i;
  if(game_solved_counter)
  {
    game_solved_counter--;
    if(!game_solved_counter)
      game_is_solved = WAIT_FOR_CHECK;
  }
  
  for(i=0; i<NO_RELAY_OUTS; i++)
  {
    if(main_config.relays_config[i].on_delay_counter)
    {
      (main_config.relays_config[i].on_delay_counter)--;
      if(!main_config.relays_config[i].on_delay_counter)
        main_config.relays_config[i].state = RELAY_DELAY_ON_EXPIRED;
    }

    if(main_config.relays_config[i].off_delay_counter)
    {
      (main_config.relays_config[i].off_delay_counter)--;
      if(!main_config.relays_config[i].off_delay_counter)
        main_config.relays_config[i].state = RELAY_DELAY_OFF_EXPIRED;
    }
  }  
}
void relay_handler(void)
{
  uint8_t i, detach_ticker = true, set_game_unsolved = true;
  for(i=0; i<NO_RELAY_OUTS; i++)
  {
    if(main_config.relays_config[i].state == RELAY_DELAY_ON_EXPIRED)
    {
      if(main_config.relays_config[i].off_delay)
      {
        Serial.print("Relay");
        Serial.print(i);
        Serial.println(" turning on");
        if(main_config.relays_config[i].nc_no == relay_NC)    // turning on relays
          io_exp.digitalWrite(relay_expios[i], 1);
        else
          io_exp.digitalWrite(relay_expios[i], 0);
        // Start turning off delay counter  
        main_config.relays_config[i].off_delay_counter = main_config.relays_config[i].off_delay;
        main_config.relays_config[i].state = RELAY_DELAY_OFF_STARTED;
      }
      else
        main_config.relays_config[i].state = RELAY_OFF;
    }

    if(main_config.relays_config[i].state == RELAY_DELAY_OFF_EXPIRED)
    {
      Serial.print("Relay");
      Serial.print(i);
      Serial.println(" turning off");
      if(main_config.relays_config[i].nc_no == relay_NC)
        io_exp.digitalWrite(relay_expios[i], 0);
      else
        io_exp.digitalWrite(relay_expios[i], 1);  

      main_config.relays_config[i].state = RELAY_OFF;
    }  
  }

  for(i=0; i<NO_RELAY_OUTS; i++)
    if(main_config.relays_config[i].on_delay_counter || main_config.relays_config[i].off_delay_counter)
    {
      detach_ticker = false;
      set_game_unsolved = false;
    }
  if(set_game_unsolved)
    game_is_solved = NOT_SOLVED;
}

void solve_game(void)
{
  uint8_t i;
  if(game_is_solved == NOT_SOLVED)
  {
    Serial.println("->Delayed solved");
    if(main_config.game_solved_delay)
    {
      game_is_solved = DELAYED_SOLVED;
      game_solved_counter = main_config.game_solved_delay;
    }
    else
      game_is_solved = WAIT_FOR_CHECK;
  }

  if(game_is_solved == WAIT_FOR_CHECK)
  {
    Serial.println("Solved");
    game_is_solved = SOLVED;
    sec_ticker.attach(1, ticker_handler);
    for(i=0; i<NO_RELAY_OUTS; i++)
    {
      if(main_config.relays_config[i].on_delay)
      {
        main_config.relays_config[i].on_delay_counter = main_config.relays_config[i].on_delay;
        main_config.relays_config[i].state = RELAY_DELAY_ON_STARTED;
      }
      else 
        main_config.relays_config[i].state = RELAY_DELAY_ON_EXPIRED;
    }
  }
}

String send_rfid_config_html()
{
  uint8_t i;
  
  String ptr = "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\"></head>";
  ptr+= "<center><b>ModuLocks RFID config</center></b><br>";
  
  ptr += "<a href=\"/\">";
  ptr += "<button>Back to main menu</button></a><br><br>";
  
  ptr+= "Select which RFID sensor you want to configurate. Cards are saved to sensor inputs, not the RFID sensors.<br><center>";

  for(i=0; i< MAX_SENSORS; i++)
  {
    ptr += "<a href=\"/set_rfid_cards?sensor_id=";
    ptr += String(i);   // sensor id is counted from 0 internally
    ptr += "\"><button>Sensor ";
    ptr += String(i+1); // but displayes from 1
    ptr += "</button></a>";
  }
  ptr += "</center>";
//  ptr += "<a href=\"/rfid_config?rfid_id=3\">";
//  ptr += "<button>Config RFID cards</button></a>";

  return ptr;
}
void rfid_config_handler()
{
  Serial.println("rfid_config");
  server.send(200, "text/html", send_rfid_config_html());
}

String send_set_rfid_cards_html(uint8_t sensor_id)
{
  uint8_t i, j;
  
  String ptr = "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\"></head>";
  ptr += "<center><b>RFID config for Sensor input " + String(sensor_id+1);
  ptr += "</center></b><br>";

  ptr += "<a href=\"/rfid_config\">";
  ptr += "<button>Back to RFID sensors</button></a><br><br>";
  
  ptr+= "Current card: <input type=\"text\" value=\"";
  for(i=0; i<last_read_cards[sensor_id].uid_length; i++)
    ptr += String(last_read_cards[sensor_id].uid[i], HEX) + " ";
  ptr += "\" readonly> ";
  ptr += "<a href=\"/set_rfid_cards?sensor_id=";
  ptr += String(sensor_id);
  ptr += "\"><button>Refresh</button></a><br><br>";

  for(i=0; i<STORED_RFID_CARDS; i++)
  {
    ptr += "<a href=\"/save_card?sensor_id=" + String(sensor_id);
    ptr += "&slot_number=" + String(i);
    ptr += "\"><button>Save --></button></a>";

    ptr += "<input type=\"text\" value=\"";
    for(j=0; j<main_config.rfid_sensor[sensor_id].cards[i].uid_length; j++)
      ptr += String(main_config.rfid_sensor[sensor_id].cards[i].uid[j], HEX) + " ";
    ptr += "\" readonly>";

    ptr += "<a href=\"/delete_card?sensor_id=" + String(sensor_id);
    ptr += "&slot_number=" + String(i);
    ptr += "\"><button>Delete</button></a>";    

    ptr += "<form action=\"/save_card_label?sensor_id=" + String(sensor_id);
    ptr += "&slot_number=" + String(i);
    ptr += "\" method=\"POST\">Label: ";
    ptr += "<input type=\"text\" name=\"card_label\" value=\"" + char_array_to_String((char*)(main_config.rfid_sensor[sensor_id].cards[i].card_name)) + "\">";
    ptr += "<input type=\"submit\" value=\"Save\">";
    ptr += "</form><br>";
  }
  return ptr;
}

void set_rfid_cards_handler()
{
  Serial.print("Sensor id: ");
  Serial.println(server.arg("sensor_id"));
  server.send(200, "text/html", send_set_rfid_cards_html(server.arg("sensor_id").toInt()));
}

void delete_card_handler()
{
  uint8_t sensor_id, slot_number, i;
  sensor_id = server.arg("sensor_id").toInt();
  slot_number = server.arg("slot_number").toInt();
  
  for(i=0; i<last_read_cards[sensor_id].uid_length; i++)
      main_config.rfid_sensor[sensor_id].cards[slot_number].uid[i] = 0;
  main_config.rfid_sensor[sensor_id].cards[slot_number].uid_length = 0;
  
  EEPROM.put(CONFIG_EEPROM_ADDR, main_config);
  EEPROM.commit(); 

  String back_address = "/set_rfid_cards?sensor_id";
  back_address += String(sensor_id);
  server.sendHeader("Location",back_address);
  server.send(303);  
}

void save_card_handler()
{
  uint8_t sensor_id, slot_number, i;
  sensor_id = server.arg("sensor_id").toInt();
  slot_number = server.arg("slot_number").toInt();

  if(last_read_cards[sensor_id].uid_length)
  {
    for(i=0; i<last_read_cards[sensor_id].uid_length; i++)
      main_config.rfid_sensor[sensor_id].cards[slot_number].uid[i] = last_read_cards[sensor_id].uid[i];
    main_config.rfid_sensor[sensor_id].cards[slot_number].uid_length = last_read_cards[sensor_id].uid_length;
    
    EEPROM.put(CONFIG_EEPROM_ADDR, main_config);
    EEPROM.commit(); 
  }
  String back_address = "/set_rfid_cards?sensor_id=";
  back_address += String(sensor_id);
  server.sendHeader("Location",back_address);
  server.send(303);  
}

void save_card_label_handler()
{
  uint8_t sensor_id, slot_number, i;
  sensor_id = server.arg("sensor_id").toInt();
  slot_number = server.arg("slot_number").toInt();

  //server.arg("card_label").toCharArray(main_config.rfid_sensor[sensor_id].cards[slot_number].card_name, MAX_CARD_LABEL_LENGTH);
  String_to_volatile_char_array(main_config.rfid_sensor[sensor_id].cards[slot_number].card_name, server.arg("card_label"), MAX_CARD_LABEL_LENGTH);
  
  EEPROM.put(CONFIG_EEPROM_ADDR, main_config);
  EEPROM.commit(); 
  
  String back_address = "/set_rfid_cards?sensor_id=";
  back_address += String(sensor_id);
  server.sendHeader("Location",back_address);
  server.send(303);  
}

//===============================================================
//                  SETUP
//===============================================================
void setup(void){
  uint8_t i;

  io_exp.pinMode(SENSOR_VDD_EN_PIN_IO_EXP, OUTPUT); // TODO optimize power consumption with this pin
  io_exp.pinMode(LED_BLUE_PIN, OUTPUT);
  io_exp.pinMode(LED_RED_PIN, OUTPUT);
  io_exp.digitalWrite(LED_BLUE_PIN, HIGH);
  
  ESP.wdtDisable();
  EEPROM.begin(NEEDED_EEPROM_SIZE);
  EEPROM.get(CONFIG_EEPROM_ADDR, main_config);
  if(main_config.all_sensor_config[0].type == 0xFF)
    reset_config();
  
  Serial.begin(115200);
  Serial.println("");
  SPI.begin();

  print_config();
  
  WiFi.mode(WIFI_AP);           //Only Access point
  WiFi.softAP(ssid, password);  //Start HOTspot removing password will disable security
  
  IPAddress myIP = WiFi.softAPIP(); //Get IP address
  Serial.print("HotSpt IP:");
  Serial.println(myIP);

  MDNS.begin("modulocks");
  server.on("/", handleRoot);      //Which routine to handle at root location
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/rfid_config", HTTP_GET, rfid_config_handler);
  server.on("/set_rfid_cards", HTTP_GET, set_rfid_cards_handler);
  server.on("/save_card", HTTP_GET, save_card_handler);
  server.on("/delete_card", HTTP_GET, delete_card_handler);
  server.on("/save_card_label", HTTP_POST, save_card_label_handler);

  httpUpdater.setup(&server);
  
  server.begin();                  //Start server

  MDNS.addService("http", "tcp", 80);
  Serial.println("HTTP server started");

  reconfig();

  for(i=0; i<NO_RELAY_OUTS; i++)
    io_exp.pinMode(relay_expios[i], OUTPUT);
  
}
//===============================================================
//                     LOOP
//===============================================================
void loop(void){
  server.handleClient();          //Handle client requests
  if(heartbeat)
  {
    io_exp.digitalWrite(LED_BLUE_PIN, LOW);
    heartbeat=0;
  }
  else
  {
    io_exp.digitalWrite(LED_BLUE_PIN, HIGH);
    heartbeat=1;
  }
  
  if(check_game_solved())
  {
    solve_game();
  } 
  else if(game_is_solved != SOLVED)
  {
    game_is_solved = NOT_SOLVED;
  }
  if(game_is_solved == SOLVED)
    relay_handler();
  delay(50);
  if(reconfig_is_needed)
  {
    reconfig();
    reconfig_is_needed = false;
  }
}

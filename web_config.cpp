#include "web_config.h"
#include <EEPROM.h>
#include "fw_version.h"

ESP8266WebServer* UIserver;

struct main_config_type main_config;
struct RFID_card last_read_cards[MAX_SENSORS];

String sensor_types[NO_SENSOR_TYPES] = {"None", "RFID", "IR", "Touch", "Hall"};
extern uint8_t reconfig_is_needed;

void print_config(void)
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

void handleConfig()
{
  uint8_t i,j;
  String sensor_list_number, on_off_number, nc_no_number, on_delay_number, off_delay_number;
  
  for(i=0; i<MAX_SENSORS; i++)
  {
    sensor_list_number = "sensor_list" + String(i);
    on_off_number = "on_off" + String(i);
    
    for(j=0; j<NO_SENSOR_TYPES; j++)
      if(UIserver->arg(sensor_list_number) == sensor_types[j])
        main_config.all_sensor_config[i].type = j;

    if(UIserver->arg(on_off_number) == "on")
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
    if(UIserver->arg(nc_no_number) == "NC")
      main_config.relays_config[i].nc_no = relay_NC;
    else
      main_config.relays_config[i].nc_no = relay_NO;    

    on_delay_number = "on_delay" + String(i);
    main_config.relays_config[i].on_delay = (UIserver->arg(on_delay_number)).toInt();

    off_delay_number = "off_delay" + String(i);
    main_config.relays_config[i].off_delay = (UIserver->arg(off_delay_number)).toInt();
  }
  main_config.game_solved_delay = (UIserver->arg("game_solved_delay")).toInt();
  
  //UIserver->arg("sound").toCharArray(main_config.sound_file, MAX_SOUND_FILENAME_SIZE);
  String_to_volatile_char_array(main_config.sound_file, UIserver->arg("sound"), MAX_SOUND_FILENAME_SIZE);
  
  //Serial.println(UIserver->arg("sound"));
  EEPROM.put(CONFIG_EEPROM_ADDR, main_config);
  EEPROM.commit();

  reconfig_is_needed = true;
  
/*  Serial.println("config handler");
  for(int i=0; i<UIserver->args(); i++)
  {
    Serial.println(UIserver->argName(i));
    Serial.println(UIserver->arg(i));
  }
  */
  UIserver->sendHeader("Location","/");
  UIserver->send(303);  
}

void reset_config(void)
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
  UIserver->send(200, "text/html", send_rfid_config_html());
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
  Serial.println(UIserver->arg("sensor_id"));
  UIserver->send(200, "text/html", send_set_rfid_cards_html(UIserver->arg("sensor_id").toInt()));
}

void delete_card_handler()
{
  uint8_t sensor_id, slot_number, i;
  sensor_id = UIserver->arg("sensor_id").toInt();
  slot_number = UIserver->arg("slot_number").toInt();
  
  for(i=0; i<last_read_cards[sensor_id].uid_length; i++)
      main_config.rfid_sensor[sensor_id].cards[slot_number].uid[i] = 0;
  main_config.rfid_sensor[sensor_id].cards[slot_number].uid_length = 0;
  
  EEPROM.put(CONFIG_EEPROM_ADDR, main_config);
  EEPROM.commit(); 

  String back_address = "/set_rfid_cards?sensor_id";
  back_address += String(sensor_id);
  UIserver->sendHeader("Location",back_address);
  UIserver->send(303);  
}

void save_card_handler()
{
  uint8_t sensor_id, slot_number, i;
  sensor_id = UIserver->arg("sensor_id").toInt();
  slot_number = UIserver->arg("slot_number").toInt();

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
  UIserver->sendHeader("Location",back_address);
  UIserver->send(303);  
}

void save_card_label_handler()
{
  uint8_t sensor_id, slot_number, i;
  sensor_id = UIserver->arg("sensor_id").toInt();
  slot_number = UIserver->arg("slot_number").toInt();

  //UIserver->arg("card_label").toCharArray(main_config.rfid_sensor[sensor_id].cards[slot_number].card_name, MAX_CARD_LABEL_LENGTH);
  String_to_volatile_char_array(main_config.rfid_sensor[sensor_id].cards[slot_number].card_name, UIserver->arg("card_label"), MAX_CARD_LABEL_LENGTH);
  
  EEPROM.put(CONFIG_EEPROM_ADDR, main_config);
  EEPROM.commit(); 
  
  String back_address = "/set_rfid_cards?sensor_id=";
  back_address += String(sensor_id);
  UIserver->sendHeader("Location",back_address);
  UIserver->send(303);  
}

//==============================================================
//     This rutine is exicuted when you open its IP in browser
//==============================================================
void handleRoot() {
  UIserver->send(200, "text/html", SendMainHTML());
}

void web_config_init(ESP8266WebServer* new_server)
{
  UIserver = new_server;

  UIserver->on("/", handleRoot);      //Which routine to handle at root location
  UIserver->on("/config", HTTP_POST, handleConfig);
  UIserver->on("/rfid_config", HTTP_GET, rfid_config_handler);
  UIserver->on("/set_rfid_cards", HTTP_GET, set_rfid_cards_handler);
  UIserver->on("/save_card", HTTP_GET, save_card_handler);
  UIserver->on("/delete_card", HTTP_GET, delete_card_handler);
  UIserver->on("/save_card_label", HTTP_POST, save_card_label_handler);

  UIserver->begin();                  //Start server
}

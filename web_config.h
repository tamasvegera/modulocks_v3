#ifndef _WEB_CONFIG_H
#define _WEB_CONFIG_H

#include "game_handler.h"
#include <ESP8266WebServer.h>

#define NEEDED_EEPROM_SIZE    2048
#define CONFIG_EEPROM_ADDR    0

#define STORED_RFID_CARDS   5
#define CARD_UID_MAX_LENGTH   10
#define MAX_SOUND_FILENAME_SIZE 64
#define MAX_CARD_LABEL_LENGTH 20

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

struct main_config_type
{
    struct SensorConfig all_sensor_config[MAX_SENSORS];
    struct RelayConfig relays_config[NO_RELAY_OUTS];
    char sound_file[MAX_SOUND_FILENAME_SIZE];
    uint8_t game_solved_delay;
    struct RFID_sensor_config rfid_sensor[MAX_SENSORS];
};

//extern struct RFID_card last_read_cards[MAX_SENSORS];

void reset_config(void);
void print_config(void);
void web_config_init(ESP8266WebServer* new_server);

#endif


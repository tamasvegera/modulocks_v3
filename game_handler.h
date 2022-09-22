#ifndef _GAME_HANDLER_H
#define _GAME_HANDLER_H

#include "clsPCA9555.h"

#define MAX_SENSORS   10
#define NO_SENSOR_TYPES 5   // add 1 extra to sensor types
#define NO_RELAY_OUTS   3

enum game_status{NOT_SOLVED, DELAYED_SOLVED, WAIT_FOR_CHECK, SOLVED};

enum relay_states{RELAY_OFF, RELAY_DELAY_ON_STARTED, RELAY_DELAY_ON_EXPIRED, RELAY_DELAY_OFF_STARTED, RELAY_DELAY_OFF_EXPIRED};
enum sensor_types_enum{SENSOR_NONE, SENSOR_RFID, SENSOR_IR, SENSOR_TOUCH, SENSOR_HALL};
enum relay_switch_states {relay_NC, relay_NO};

void reconfig(void);
void set_io_exp_for_game_handler(PCA9555* new_io_exp);
void game_loop(void);

#endif


#include <Arduino.h>
#include "require_cpp11.h"
#include "game_handler.h"
#include "clsPCA9555.h"
#include "MFRC522_.h"
#include "web_config.h"
#include <Ticker.h>
#include "hw.h"


//PCA9555* io_exp;
extern struct main_config_type main_config;
extern PCA9555 io_exp;
extern struct RFID_card last_read_cards[MAX_SENSORS];

enum game_status game_is_solved = NOT_SOLVED;
uint8_t game_solved_counter = 0;

Ticker sec_ticker;

MFRC522 *rfids[MAX_SENSORS];

uint8_t io_to_expio[MAX_SENSORS] = {15, 14, 13, 12, 11, 10, 8, 9, 6, 7};
uint8_t relay_expios[NO_RELAY_OUTS] = {1, 2, 3};
uint8_t rfids_present[MAX_SENSORS];

/*void set_io_exp_for_game_handler(PCA9555* new_io_exp)
{
  io_exp = new_io_exp;
}*/

void reconfig(void)
{
  uint8_t i;
  Serial.println("Reconfig...");

  reset_sensor_vdd();
  
  for(i=0; i<MAX_SENSORS; i++)
  {
    delete rfids[i];
    switch(main_config.all_sensor_config[i].type)
    {
      case SENSOR_NONE:
        io_exp.pinMode(io_to_expio[i], INPUT);
        break;
      case SENSOR_RFID:
        rfids[i] = new MFRC522(/*&io_exp,*/ io_to_expio[i], 0xFF);
        rfids[i]->PCD_Init();
        //delay(100);
        rfids_present[i] = (rfids[i] -> PCD_IsPresent());

        Serial.print("RFID");
        Serial.print(i+1);
        Serial.print(" is present:");
        Serial.println(rfids_present[i]);
        
        //delay(100);
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

uint8_t check_game_solved(void)
{
  uint8_t game_solved = true, i, j, k, current_card_matched, temp_card_present, temp_uid_read;
  for(i=0; i<MAX_SENSORS; i++)
  {
    switch(main_config.all_sensor_config[i].type)
    {
      case SENSOR_RFID:
        if(!rfids_present[i])
        {
          // TODO if not present but it should be?
          break;
        }

        temp_card_present = rfids[i]->PICC_IsNewCardPresent();
        temp_uid_read = rfids[i]->PICC_ReadCardSerial();

        if(temp_card_present && temp_uid_read)
        {
          // copy current read ID to last_read_cards
          for(j=0; j<rfids[i]->uid.size; j++)
            last_read_cards[i].uid[j] = rfids[i]->uid.uidByte[j];
          last_read_cards[i].uid_length = rfids[i]->uid.size;          
        }
        else
        {
          last_read_cards[i].uid_length = 0;
          for(j=0; j<CARD_UID_MAX_LENGTH; j++)
            last_read_cards[i].uid[j] = 0x00;
        } 
        
        // TODO simplify this if-else if - if - else
        if(temp_card_present != main_config.all_sensor_config[i].on_off)    // XOR
        {
            game_solved = false;
//          delay(50);
        }        
        else if(temp_uid_read != main_config.all_sensor_config[i].on_off)
        {
          game_solved = false;
          //delay(50);
        }
        if(game_solved)
        {
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

void game_loop(void)
{ 
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
}


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include "MFRC522_.h"
#include <Wire.h>
#include "clsPCA9555.h"
#include <SPI.h>
#include <EEPROM.h>
#include "web_config.h"
#include "game_handler.h"
#include "hw.h"

#define LED_BLUE_PIN  4
#define LED_RED_PIN 5

extern PCA9555 io_exp(0x20);

extern uint8_t relay_expios[NO_RELAY_OUTS];
extern struct main_config_type main_config;

//SSID and Password to your ESP Access Point
const char* ssid = "ESPWebServer";
const char* password = "12345678";

uint8_t reconfig_is_needed = false;

uint8_t heartbeat=0;

ESP8266WebServer server(80); //Server on port 80
ESP8266HTTPUpdateServer httpUpdater;

void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

//===============================================================
//                  SETUP
//===============================================================
void setup(void){
  uint8_t i;

  reset_sensor_vdd();
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

  pinMode(12, INPUT);
  SPI.begin();

  print_config();
  
  WiFi.mode(WIFI_AP);           //Only Access point
  WiFi.softAP(ssid, password);  //Start HOTspot removing password will disable security
  
  IPAddress myIP = WiFi.softAPIP(); //Get IP address
  Serial.print("HotSpt IP:");
  Serial.println(myIP);

  MDNS.begin("modulocks");

  httpUpdater.setup(&server);
  web_config_init(&server);
  
  MDNS.addService("http", "tcp", 80);
  Serial.println("HTTP server started");

  reconfig();

  for(i=0; i<NO_RELAY_OUTS; i++)
    io_exp.pinMode(relay_expios[i], OUTPUT);

  //set_io_exp_for_game_handler(&io_exp);
}
//===============================================================
//                     LOOP
//===============================================================
void loop(void){
  ESP.wdtFeed();
  
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

  delay(50);
  if(reconfig_is_needed)
  {
    reconfig();
    reconfig_is_needed = false;
  }

  game_loop();
}

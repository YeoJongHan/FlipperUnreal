//-------------------------------------------------------------------------------
//  TinyCircuits ST BLE TinyShield UART Example Sketch
//  Last Updated 2 March 2016
//
//  This demo sets up the BlueNRG-MS chipset of the ST BLE module for compatiblity 
//  with Nordic's virtual UART connection, and can pass data between the Arduino
//  serial monitor and Nordic nRF UART V2.0 app or another compatible BLE
//  terminal. This example is written specifically to be fairly code compatible
//  with the Nordic NRF8001 example, with a replacement UART.ino file with
//  'aci_loop' and 'BLEsetup' functions to allow easy replacement. 
//
//  Written by Ben Rose, TinyCircuits http://tinycircuits.com
//
//-------------------------------------------------------------------------------


#include <SPI.h>
#include <STBLE.h>
#include <cstdlib>
#include <cstring>

//Debug output adds extra flash and memory requirements!
#ifndef BLE_DEBUG
#define BLE_DEBUG true
#endif

#if BLE_DEBUG
#include <stdio.h>
char sprintbuff[100];
#define PRINTF(...) {sprintf(sprintbuff,__VA_ARGS__);SerialMonitorInterface.print(sprintbuff);}
#else
#define PRINTF(...)
#endif

#if defined (ARDUINO_ARCH_AVR)
#define SerialMonitorInterface Serial
#elif defined(ARDUINO_ARCH_SAMD)
#define SerialMonitorInterface SerialUSB
#endif

void nth() {PRINTF("NOTHING\n");}

typedef void (*FuncType)(); 
// List of functions for each menu
FuncType menus[1024]{};

uint8_t ble_rx_buffer[21];
uint8_t ble_rx_buffer_len = 0;
uint8_t ble_connection_state = false;
uint16_t menuMode = 0;
uint8_t attackCount = 1;
char message[21]{};

int connected = FALSE;
int buttonPressed = FALSE;
int buttonChoice = -1;

#define PIPE_UART_OVER_BTLE_UART_TX_TX 0

void setup() {
  SerialMonitorInterface.begin(9600);
  while (!SerialMonitorInterface); //This line will block until a serial monitor is opened with TinyScreen+!
  BLEsetup();
  screenSetup();
  writeDisconnected();
  setupMenuList();
}

void setupMenuList() {
  // {prevMenu, AttackMenu, RecordMenu, ListMenu, CurrFuncMenu, attackFunc, SecondFunc, ThirdFunc}
  for (int i = 0; i < 1024; ++i) {
    menus[i] = nth;
  }
  menus[0b00] = writeMenu;
  menus[0b01] = writeIRMenu;
  menus[0b10] = writeRFMenu;
  menus[0b11] = writeRFIDMenu;

  menus[0b0101] = writeAIRMenu;
  menus[0b0110] = writeRIRMenu;

  menus[0b011001] = writeAttackCountMenu;
  menus[0b01100101] = changeAttackCount;
  menus[0b01100110] = changeAttackCount;
  menus[0b01100111] = startListen;

  menus[0b011010] = playIRRecord;

}

void sendDataBLE(uint8_t *sendBuffer, uint8_t sendLength) {
  sendBuffer[sendLength] = '\0'; //Terminate string
  sendLength++;
  if (!lib_aci_send_data(PIPE_UART_OVER_BTLE_UART_TX_TX, (uint8_t*)sendBuffer, sendLength))
  {
    SerialMonitorInterface.println(F("TX dropped!"));
  }
}

void changeAttackCount() {
  // If low bit is set, means increase, else decrease
  attackCount += (menuMode & 0b01) ? -1 : 1;
  menuMode >>= 2;
  writeAttackCount();
}

void playIRRecord() {
  uint8_t sendBuffer[21];
  writeMessage("Starting...");

  sendBuffer[0] = menuMode & 255;
  sendBuffer[1] = menuMode >> 8;
  
  sendDataBLE(sendBuffer, 2);
}

void startListen(){
  uint8_t sendBuffer[21];
  writeMessage("Starting...");

  sendBuffer[0] = menuMode & 255;
  sendBuffer[1] = menuMode >> 8;
  sendBuffer[2] = attackCount;
  
  sendDataBLE(sendBuffer, 3);
}

void stopAttack(){
  uint8_t sendBuffer[21];
  attackCount = 1;

  sendBuffer[0] = 'H';
  sendBuffer[1] = 'A';
  sendBuffer[2] = 'L';
  sendBuffer[3] = 'T';
  
  sendDataBLE(sendBuffer, 4);
}

void backMenu(int count=1) {
  menuMode >>= (count * 2);
  menus[menuMode]();
}

void loop() {
  aci_loop();//Process any ACI commands or events from the NRF8001- main BLE handler, must run often. Keep main loop short.
  if (ble_rx_buffer_len) {//Check if data is available
    SerialMonitorInterface.print(ble_rx_buffer_len);
    SerialMonitorInterface.print(" : ");
    SerialMonitorInterface.println((char*)ble_rx_buffer);

    if (strncmp((char*)ble_rx_buffer, "OK", 2) == 0) {
      if (*(uint16_t*)(ble_rx_buffer + 2) == 0b01100111) {
        writeExit();
        strncpy(message, "Press key ", 10);
        strcpy(message + 10, (char*)ble_rx_buffer + 4);
        writeCenter(message);
        memset(message, 0, 21);
      } else if (*(uint16_t*)(ble_rx_buffer + 2) == 0b011010) {
        writeMessage("Playing...");
      } else {
        writeMessage("Unknown attack");
        stopAttack();
        delay(1000);
        backMenu(2);
      }
    } else if (strncmp((char*)ble_rx_buffer, "SU", 2) == 0) {
      writeMessage("Success!");
      attackCount = 1;
      delay(1000);
      backMenu(2);
    } else {
      writeMessage("Error!");
      stopAttack();
      delay(1000);
      backMenu(2);
    }

    memset(ble_rx_buffer, 0, sizeof(ble_rx_buffer));
    ble_rx_buffer_len = 0;//clear after reading
  }

  if (connected) {
    buttonChoice = buttonLoop();
    // drawMenuGif();
    if (buttonChoice != -1) {
      if ((buttonChoice ? ((menuMode << 2) | buttonChoice) : (menuMode >> 2)) < (sizeof(menus)/sizeof(menus[0]))) {
        if (!buttonChoice && menuMode == 0b01100111) {
          stopAttack();
        } else {
          menuMode = buttonChoice ? ((menuMode << 2) | buttonChoice) : (menuMode >> 2);
          PRINTF("RUNNING %d %d\n", menuMode, (sizeof(menus)/sizeof(menus[0])));
          menus[menuMode]();
        }
      } else {
        PRINTF("ELSE PRINTED\n");
      }
    }
  } else {
    writeDisconnected();
  }
}


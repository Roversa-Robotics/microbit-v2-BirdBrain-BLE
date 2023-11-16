#ifndef BIRDBRAIN_H
#define BIRDBRAIN_H

#include "MicroBit.h"
#include "MicroBitUARTService.h"

extern MicroBit uBit;
extern MicroBitUARTService *bleuart;
extern char initials_name[10]; // Holds our fancy name initials
extern uint8_t whatAmI; // Holds whether the device is currently in standalone micro:bit, Finch, or Hummingbird mode 
extern bool bleConnected; // Holds if connected over BLE
extern bool notifyOn; // Holds if notifications are on (we are regularly sending sensor packets back)
extern bool flashOn;
extern int32_t leftEncoder; //Holds the running value of the left encoder
extern int32_t rightEncoder; //Holds the running value of the right encoder

extern bool leftMotorMove; // Flag to find out if the motor is currently moving
extern bool leftMotorForwardDirection; // Flag to tell us which direction it's moving
extern bool rightMotorMove; // Same as above, for the right motor
extern bool rightMotorForwardDirection;

extern uint16_t sleepCounter; // Used to check if we should turn off the Finch due to lack of BLE activity

#include "SpiControl.h"
#include "Naming.h"
#include "BBMicroBit.h"
#include "BLESerial.h"



// Codes for which type of device we are - a micro:bit, Finch, or Hummingbird
#define A_MB           0
#define A_HB           1
#define A_FINCH        2

// Setting up BB specific events
#define BB_ID            MICROBIT_ID_NOTIFY+1 // last defined eventId is MICROBIT_ID_NOTIFY==1023 in MicroBitComponent.h 
#define FLASH_MSG_EVT  1
#define MB_BUZZ_EVT    2

// Time out for Finch to disconnect and turn off if it has not received a command. Currently set to 10 minutes
#define FINCH_INACTIVITY_TIMEOUT                           10


// disabling the event service as we don't need it
//#ifndef MICROBIT_BLE_EVENT_SERVICE
//#define MICROBIT_BLE_EVENT_SERVICE              0
//#endif

#endif


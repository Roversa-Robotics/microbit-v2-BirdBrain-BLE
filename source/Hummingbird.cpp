#include "MicroBit.h"
#include "BirdBrain.h"
#include "SpiControl.h"
#include "BBMicroBit.h"
#include "Hummingbird.h"
#include "BLESerial.h"

// Initializes the Hummingbird, mostly setting the edge connector pins as we want
void initHB()
{
    // Setting the buzzer, and HB LED ports 2 and 3 to 0
    uBit.io.P0.setAnalogValue(0);
    uBit.io.P2.setAnalogValue(0);
    uBit.io.P8.setAnalogValue(0);

    // Sending a stop command just in case
    stopHB();
}

// Sends the stop command to the Hummingbird
void stopHB()
{
    uint8_t stopCommand[4] = {STOP_ALL, 0xFF, 0xFF, 0xFF};
    // Setting the buzzer, and HB LED ports 2 and 3 to 0
    uBit.io.P0.setAnalogValue(0);
    uBit.io.P2.setAnalogValue(0);
    uBit.io.P8.setAnalogValue(0);    
}

// Sets all Hummingbird outputs in one go
void setAllHB(uint8_t commands[], uint8_t length)
{
    // Double check that the command contains enough data for us to proceed
    if(length >= HB_SETALL_LENGTH)
    {
        // Setting the edge connector LEDs
        uBit.io.P2.setAnalogValue(commands[13]*4);
        uBit.io.P8.setAnalogValue(commands[14]*4);        
        // setting the buzzer
        uint16_t buzzPeriod = (commands[15]<<8) + commands[16];
        uint16_t buzzDuration = (commands[17]<<8) + commands[18];
        setBuzzer(buzzPeriod, buzzDuration);
        // Sending the SPI command to control the remaining LEDs + servos - 13 bytes
    }
}
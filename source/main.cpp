#include "MicroBit.h"
#include "MicroBitUARTService.h"

#include "BirdBrain.h"
#include "SpiControl.h"
#include "Naming.h"
#include "Hummingbird.h"
#include "Finch.h"

#include <cstdio>
#include "Pins.h"
#include "Notes.h"
#include "BBMicroBit.h"

MicroBit uBit;

// Check for BLE data, execute appropriate commands
// sensor data is sent asynchronously in a different fiber
void ble_mgmt_loop()
{
    while (1)
    {                       // loop for ever
        bleSerialCommand(); // reads the serial command and then executes on that command
        fiber_sleep(1);
    }
}

// Checks if the device type has changed once per second if not connected
// If you plug in the micro:bit to a HB or Finch after powering it on, the advertised name changes
void check_device_loop()
{

    uint8_t past_device;
    uint8_t current_device = 0;
    bool update;
    ManagedString devicePrefix;

    // initializing current device to whatever it is at start up
    switch (whatAmI)
    {
    case A_MB:
        current_device = MICROBIT_SAMD_ID;
        // will need to init MB to make sure the edge connector isn't left in a poor state
        break;
    case A_HB:
        current_device = HUMMINGBIT_SAMD_ID;
        break;
    case A_FINCH:
        current_device = FINCH_SAMD_ID;
        // will need to init Finch and make sure reset line is low
        break;
    }

    while (1)
    {
        // If you're connected to a tablet/computer, we're not changing your prefix in mid-use, so
        // only do the following if you're not connected
        if (!bleConnected)
        {
            past_device = current_device;           // what device do we think we are
            current_device = readFirmwareVersion(); // what are we really?
            // If we are not what we think, then update the device name
            if (past_device != current_device)
            {
                update = true;
                switch (current_device)
                {
                case MICROBIT_SAMD_ID:
                    devicePrefix = "MB";
                    whatAmI = A_MB;
                    initials_name[3] = 'M';
                    initials_name[4] = 'B';
                    break;
                case FINCH_SAMD_ID:
                    devicePrefix = "FN";
                    whatAmI = A_FINCH;
                    initials_name[3] = 'F';
                    initials_name[4] = 'N';
                    break;
                case HUMMINGBIT_SAMD_ID:
                    devicePrefix = "BB";
                    whatAmI = A_HB;
                    initHB();
                    initials_name[3] = 'B';
                    initials_name[4] = 'B';
                    break;
                default: // only update if you read SPI correctly
                    update = false;
                    break;
                }
                // Update the GAP name over BLE
                if (update)
                {
                    uBit.ble->stopAdvertising();
                    fiber_sleep(10);
                    uBit.ble->configAdvertising(devicePrefix);
                    fiber_sleep(10);
                    uBit.ble->advertise();
                }
            }
        }
        fiber_sleep(1000);
    }
}

void pause()
{

    fiber_sleep(10);
}

int main()
{

    uBit.init(); // Initializes everything but SPI
    
    // Set the buzzer pin low so we don't accidentally energize the Finch or HB buzzer
    uBit.io.P0.setDigitalValue(0);

    // Toggle the reset pin on the Finch, then hold it low
    // This happens even for HB and standalone micro:bit, as it needs to happen before we can
    // determine device type
    uBit.io.pin[RESET_PIN].setDigitalValue(1);
    fiber_sleep(200);
    uBit.io.pin[RESET_PIN].setDigitalValue(0);

    // Wait for the SAMD bootloader checker
    fiber_sleep(1850);
     // Figure out what we are called, start flashing our initials
    getInitials_fancyName();

    // Get our name prefix - BB, FN, or MB - depending on what we are attached to
    ManagedString bbDevName = whichDevice();

    // Wait for the BLE stack to stabilize before registering the UART service
    fiber_sleep(10);

    // Start up a UART service and start advertising
    bleSerialInit(bbDevName);

    // Setting up an event listener for flashing messages and for running the buzzer
    BBMicroBitInit();

    uBit.serial.send("BLE Mgmt Loop\n\r");
    // Creating the main fiber that listens for BLE messages
    create_fiber(ble_mgmt_loop);
    // Create a fiber to check if you plugged in or unplugged your micro:bit to a Finch or
    // Hummingbird
    uBit.serial.send("Check device loop\n\r");
    create_fiber(check_device_loop);

    uBit.serial.send("Releasing fiber...\n\r");
    release_fiber();
}

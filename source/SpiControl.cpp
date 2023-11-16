#include "MicroBit.h"
#include "BirdBrain.h"
#include "SpiControl.h"
#include "Hummingbird.h"
#include "Finch.h"
#include "BLESerial.h"

SPI spi(MOSI, MISO, SCK);
uint8_t whatAmI = 0;
bool spiActive; // ensures we do not accidentally interleave SPI commands called from different
                // fibers

ManagedString whichDevice()
{
    ManagedString devicePrefix = "";

    // Reading the firmware version, doing this twice as the first time it seems like values are
    // junk
    readFirmwareVersion();
    readFirmwareVersion();

    uint8_t RETRIES = 2;
    for (int i = 0; i < RETRIES; i++)
    {
        switch (readFirmwareVersion())
        {
        case MICROBIT_SAMD_ID:
            devicePrefix = "MB";
            whatAmI = A_MB;
            break;
        case FINCH_SAMD_ID:
            devicePrefix = "FN";
            whatAmI = A_FINCH;
            // initFinch();
            break;
        case HUMMINGBIT_SAMD_ID:
            devicePrefix = "BB";
            whatAmI = A_HB;
            initHB();
            break;
        }
    }

    // If the value is still junk, call it a standalone micro:bit
    if (devicePrefix != "")
    {
        devicePrefix = "MB";
        whatAmI = A_MB;
    }

    return devicePrefix;
}

uint8_t readFirmwareVersion()
{
    // Returning early to spoof the device type
    return FINCH_SAMD_ID;

}

// Function for debugging use only
void printFirmwareResponse()
{
    if (uBit.buttonA.isPressed())
    {
        bleConnected = true;

        uBit.io.P16.setDigitalValue(0);
        NRFX_DELAY_US(SS_WAIT);
        uint8_t readBuffer[4];
        readBuffer[0] = spi.write(
            0x8C); // Special command to read firmware/hardware version for both Finch and HB
        NRFX_DELAY_US(WAIT_BETWEEN_BYTES);
        for (int i = 1; i < 3; i++)
        {
            readBuffer[i] = spi.write(0xFF);
            NRFX_DELAY_US(WAIT_BETWEEN_BYTES);
        }
        readBuffer[3] = spi.write(0xFF);
        NRFX_DELAY_US(SS_WAIT);
        uBit.io.P16.setDigitalValue(1);
        NRFX_DELAY_MS(1); // wait after reading firmware

        for (int i = 0; i < 4; i++)
        {
            uBit.display.printAsync(readBuffer[i]);
            fiber_sleep(1400);
            uBit.display.clear();
            fiber_sleep(200);
        }
        bleConnected = false;
    }
}
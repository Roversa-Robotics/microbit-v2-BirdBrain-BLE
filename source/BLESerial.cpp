#include "MicroBit.h"
#include "BirdBrain.h"
#include "BLESerial.h"
#include "Hummingbird.h"
#include "Finch.h"
#include "SpiControl.h"

static NRF52ADCChannel *mic = NULL; // Used to increase the gain of the mic ADC channel

bool bleConnected = false;       // Holds if connected over BLE
bool notifyOn = false;           // Holds if notifications are being sent
bool calibrationSuccess = false; // Holds if calibration succeeded
bool calibrationAttempt = false; // Holds if we've made a calibration attempt yet
MicroBitUARTService *bleuart;
bool processCommand =
    false; // Flag to hold off sending sensor data until we have first processed an inbound command
bool v2report = false; // Flag to hold if we are reporting V2 data

// Checks if the Finch has received any BLE messages recently - if not, Finch will go to sleep
uint16_t sleepCounter = 0;

// Holds length of the inbound packet buffer
uint8_t bufferLength = 0;

int16_t micSamples[MIC_SAMPLES]; // Holds 8 samples of microphone data to determine loudness
uint8_t loudness; // Holds the loudness of microphone - the difference between the min and max of
                  // the 8 samples

// Convenience function to read the command packet, returns false if it timed out
bool getCommands(uint8_t commands[], uint8_t startIndex, uint8_t length);

// Convenience function to read a single byte, index is the position in the array where we want to
// put the byte
bool readOneByte(uint8_t commands[], int index);

// Function to get the loudness of the set of microphone samples
void getLoudnessVal();

// Sends BLE sensor data approx every 30-40 ms (varies a bit on what else is going on)
// Also reads the microphone once per 4 millisecond
void send_ble_data()
{
    uint8_t loopCount = 0;
    // uint16_t elapsedTime;
    // long int previousTime = uBit.systemTime();

    while (notifyOn)
    {
        if (v2report)
        {
            // Read the microphone sound value
            micSamples[loopCount] = uBit.io.microphone.getAnalogValue();
        }
        fiber_sleep(1); // this appears to actually take 4 ms in our testing
        loopCount++;
        if (loopCount >= MIC_SAMPLES)
        {
            loopCount = 0;
            if (v2report)
            {
                getLoudnessVal();
            }
            assembleSensorData(); // assembles and sends a sensor packet
            // Used for testing the actual time between sensor packets
            /* elapsedTime = uint16_t(uBit.systemTime()-previousTime);
             previousTime = uBit.systemTime();

             uBit.serial.send(elapsedTime);
             uBit.serial.send("\r\n");*/
        }
    }
    release_fiber();
}

// Looks for the maximum difference in the samples we have taken
void getLoudnessVal()
{
    int16_t low = 10000;
    int16_t high = -10000;

    // Iterate through the microphone samples
    for (int i = 0; i < MIC_SAMPLES; i++)
    {
        if (micSamples[i] < low)
            low = micSamples[i];
        if (micSamples[i] > high)
            high = micSamples[i];
    }
    // Cramming the data into one byte by truncating anything over 255
    if ((high - low) > 255)
        loudness = 255;
    else
        loudness = high - low;
}

void onConnected(MicroBitEvent)
{
    bleConnected = true;
    playConnectSound();
}

void onDisconnected(MicroBitEvent)
{
    bleConnected = false;
    notifyOn = false; // in case this was not reset by the computer/tablet
    flashOn = false;  // Turning off any current message being printed to the screen
    stopMB(); // Stops the LED screen and buzzer, and if a MB sets edge connector pins to inputs
    playDisconnectSound();
    if (whatAmI == A_FINCH)
    {
        stopFinch();
    }
    if (whatAmI == A_HB)
    {
        stopHB();
    }
    // Turn off the microphone
    if (v2report)
    {
        uBit.io.runmic.setDigitalValue(0);
    }
}

void sleepTimer()
{
    // Checks if we should turn off the Finch
    bool playDisconnectWhenTimedOut = true;
    while (1)
    {
        fiber_sleep(60000); // wait one minute
        if (whatAmI == A_FINCH)
        {
            sleepCounter++;
            // Currently shuts down after ten minutes
            if (sleepCounter > FINCH_INACTIVITY_TIMEOUT)
            {
                // To keep the disconnect sound from playing multiple times
                if (playDisconnectWhenTimedOut)
                {
                    playDisconnectSound(); // play a sound to tell people we're turning off
                    playDisconnectWhenTimedOut = false;
                }
                turnOffFinch();
            }
        }
    }
}

void flashInitials()
{
    uint8_t count = 0;
    // MicroBitImage
    // flash("255,255,255,255,255\n255,255,255,255,255\n255,255,255,255,255\n255,255,255,255,255\n255,255,255,255,255\n");
    bool displayOnlyFancy = false;
    bool displayOnlyBrowser = false;
    while (1)
    {
        if (!bleConnected)
        {
            if (uBit.buttonA.isPressed())
            {
                count = 0;
                uBit.display.clear();
                fiber_sleep(900);
                displayOnlyFancy = true;
                displayOnlyBrowser = false;
            }

            if (uBit.buttonB.isPressed())
            {
                count = 3;
                uBit.display.clear();
                fiber_sleep(900);
                displayOnlyFancy = false;
                displayOnlyBrowser = true;
            }
            // Print one of three initials
            uBit.display.printAsync(initials_name[count]);
            fiber_sleep(500);
            uBit.display.clear();
            fiber_sleep(100);
            count++;
            // If you're at 3, spend a longer time with display cleared so it's obvious which
            // initial is first
            if (count == 3)
            {
                fiber_sleep(300);

                if (displayOnlyFancy == true)
                {
                    count = 0;
                }
                else
                {
                    uBit.display.printAsync('#');
                    fiber_sleep(500);
                    uBit.display.clear();
                }
                fiber_sleep(500);
            }
            // If you're at 10, spend a longer time with display cleared and reset the counter
            if (count == 10)
            {
                // uBit.display.printAsync(flash);
                // fiber_sleep(500);
                uBit.display.clear();
                fiber_sleep(900);
                if (displayOnlyBrowser == true)
                    count = 3;
                else
                    count = 0;
            }
        }
        else
        {
            fiber_sleep(1000);
            count = 0;
        }
    }
}

// Initializes the UART
void bleSerialInit(ManagedString devName)
{
    bleuart = new MicroBitUARTService(
        *uBit.ble, 240, 32); // increasing the buffer size to catch more near simultaneous commands

    // uBit.ble->stopAdvertising();

    fiber_sleep(10); // Give the UART service a moment to register

    // Configure advertising with the UART service added, and with our prefix added
    uBit.ble->configAdvertising(devName);

    // Waiting for the BLE stack to stabilize
    fiber_sleep(100);

    uBit.ble->setTransmitPower(7);
    uBit.ble->advertise();

    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_CONNECTED, onConnected,
                           MESSAGE_BUS_LISTENER_REENTRANT);
    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_DISCONNECTED, onDisconnected,
                           MESSAGE_BUS_LISTENER_REENTRANT);
    create_fiber(flashInitials); // Start flashing since we're disconnected
    create_fiber(
        sleepTimer);  // Start a fiber to check if we need to switch off the Finch due to inactivity
    v2report = false; // making sure we start in this state
}

// Checks what command (setAll, get firmware, etc) is coming over BLE, then acts as necessary

void bleSerialCommand()
{
    // This tracks how many bytes we've used. Not really used at the moment
    uint8_t bytesUsed = 0;

    // Run this loop if there is data in the buffer
    // This allows multiple commands to execute sequentially since it just gets called over and over
    // in the main while loop
    if (bleConnected && bleuart->isReadable() && (processCommand == false))
    {
        processCommand =
            true; // set a flag that tells the sensor packet function not to interrupt this
        bufferLength =
            bleuart->rxBufferedSize(); // Get the length of the buffer, can contain multiple packets

        uint8_t ble_read_buff[bufferLength];    // local buffer to hold our command packet
        memset(ble_read_buff, 0, bufferLength); // resetting the buffer
        bleuart->read(ble_read_buff, bufferLength, ASYNC); // read the entire buffer
        bleuart->resetBuffer(); // resets the buffer as we have read everything, not doing this
                                // seemed to cause issues
        uint8_t commandCount = 0;

        // for debugging only, to inspect BLE packets
        uBit.serial.sendChar(bufferLength, SYNC_SLEEP);
        if (ble_read_buff[0] == 0xD2)
        {
            // uBit.serial.send("MOOTTTTOOORRRRR\n");
        }
        for (int i = 0; i < bufferLength; i++)
        {
            // uBit.serial.sendChar(ble_read_buff[i], SYNC_SLEEP);

            // uBit.serial.printf("%x,", ble_read_buff[i]);
            uBit.serial.sendChar(ble_read_buff[i],SYNC_SLEEP);
        }
        uBit.serial.sendChar(0xEE, SYNC_SLEEP);
        uBit.serial.sendChar(0xEE, SYNC_SLEEP);
        uBit.serial.send("\n");
        while (commandCount < bufferLength)
        {
            sleepCounter = 0; // reset the sleep counter since we have received a command

            // Switch on the first command byte
            switch (ble_read_buff[commandCount])
            {
            case SET_LEDARRAY:
                if (whatAmI == A_MB || whatAmI == A_HB)
                {
                    // If our command is to print a symbol, then we're using 6 bytes
                    if (ble_read_buff[commandCount + 1] == SYMBOL)
                    {
                        bytesUsed = 6; // 2 bytes for commands, 4 for the symbol
                    }
                    else if (ble_read_buff[commandCount + 1] & SCROLL)
                    {
                        bytesUsed = (ble_read_buff[1] & 0x1F) +
                                    2; // We use 2 bytes + the length of the message
                    }
                    else
                    {
                        bytesUsed = 2; // If this happens, it is a clear screen command
                    }
                    // get the rest of the command packet, only do something with it if you have
                    // enough data
                    uint8_t packetCommands[bytesUsed];
                    if (bufferLength >= commandCount + bytesUsed)
                    {
                        for (int i = 0; i < bytesUsed; i++)
                        {
                            packetCommands[i] = ble_read_buff[i + commandCount];
                        }
                        decodeAndSetDisplay(packetCommands, bytesUsed);
                        commandCount += bytesUsed;
                    }
                    else
                    {
                        commandCount++;
                    }
                }
                else
                {
                    commandCount++;
                }
                break;
            // Returns the firmware and hardware versions
            case SET_FIRMWARE:
            case FINCH_SET_FIRMWARE:
                returnFirmwareData();
                commandCount++;
                break;
            // Command to start or stop sensor notifications
            case NOTIFICATIONS:
                commandCount++;
                if (bufferLength > commandCount)
                {
                    if (ble_read_buff[commandCount] == START_NOTIFY)
                    {
                        v2report = false;
                        notifyOn = true;
                        create_fiber(send_ble_data); // Sends sensor data every 30 ms
                        commandCount++;
                        // In the unlikely event that we go from reporting V2 style reports to V1
                        // without stopping notifications
                        if (v2report)
                        {
                            uBit.io.runmic.setDigitalValue(0);
                        }
                    }
                    // Send V2 compatible reports
                    else if (ble_read_buff[commandCount] == START_NOTIFYV2)
                    {
                        v2report = true;
                        notifyOn = true;
                        create_fiber(send_ble_data); // Sends sensor data every 30 ms
                        // Increase the gain of the microphone ADC
                        if (mic == NULL)
                        {
                            mic = uBit.adc.getChannel(uBit.io.microphone);
                            mic->setGain(7, 0);
                        }
                        // Power up the microphone
                        uBit.io.runmic.setDigitalValue(1);
                        uBit.io.runmic.setHighDrive(true);
                        commandCount++;
                    }
                    else if (ble_read_buff[commandCount] == STOP_NOTIFY)
                    {
                        notifyOn = false;
                        commandCount++;
                        if (v2report)
                        {
                            uBit.io.runmic.setDigitalValue(0);
                        }
                    }
                    bytesUsed = 2; // Uses two bytes
                }
                break;
            case MICRO_IO:
                if (whatAmI == A_MB)
                {
                    bytesUsed = 8; // This command always uses 8 bytes
                    uint8_t packetCommands[bytesUsed];
                    if (bufferLength >= commandCount + bytesUsed)
                    {
                        for (int i = 0; i < bytesUsed; i++)
                        {
                            packetCommands[i] = ble_read_buff[i + commandCount];
                        }
                        decodeAndSetPins(packetCommands);
                        commandCount += bytesUsed;
                    }
                    else
                    {
                        commandCount++;
                    }
                }
                else
                {
                    commandCount++;
                }
                break;
            case STOP_ALL:
                stopMB(); // Stops the LED screen and buzzer, and if a MB sets edge connector pins
                          // to inputs
                if (whatAmI == A_HB)
                {
                    stopHB(); // stops servos and LEDs on the HB
                }
                commandCount++; // incrementing by 1 because sometimes only one byte is sent
                bytesUsed = 4; // This command sends 0xCB followed by 0xFF three times (or sometimes
                               // just 0xCB)

                break;
            case SET_CALIBRATE:
                notifyOn = false; // Turn off sensor notifications
                uBit.compass.calibrate();
                calibrationAttempt = true;
                calibrationSuccess = uBit.compass.isCalibrated();
                notifyOn = true;             // restart notifications
                create_fiber(send_ble_data); // Restart the notification fiber
                commandCount++;
                bytesUsed = 4; // This command sends 0xCE followed by 0xFF three times (or sometimes
                               // just 0xCE)
                break;
            // Sets the Hummingbird outputs and, in some cases, the micro:bit's buzzer
            case SETALL_SPI:
                if (bufferLength >= commandCount + HB_SETALL_LENGTH)
                {
                    if (whatAmI == A_HB)
                    {
                        bytesUsed = HB_SETALL_LENGTH; // 19 bytes right now
                        uint8_t packetCommands[bytesUsed];
                        for (int i = 0; i < bytesUsed; i++)
                        {
                            packetCommands[i] = ble_read_buff[i + commandCount];
                        }
                        setAllHB(packetCommands, bytesUsed); // Sets all outputs + buzzer
                        commandCount += bytesUsed;
                    }
                    // Allow this command to set the V2's onboard buzzer in standalone mode, for
                    // Snap! compatibility
                    else if (whatAmI == A_MB)
                    {
                        bytesUsed = HB_SETALL_LENGTH; // 19 bytes right now
                        // setting the buzzer
                        uint16_t buzzPeriod = (ble_read_buff[commandCount + 15] << 8) +
                                              ble_read_buff[commandCount + 16];
                        uint16_t buzzDuration = (ble_read_buff[commandCount + 17] << 8) +
                                                ble_read_buff[commandCount + 18];
                        setBuzzer(buzzPeriod, buzzDuration);
                        commandCount += bytesUsed;
                    }
                    else
                    {
                        commandCount++;
                    }
                }
                else
                {
                    commandCount++;
                }
                break;
            // Sets the Finch LEDs + buzzer
            case FINCH_SETALL_LED:
                if (whatAmI == A_FINCH && (bufferLength >= commandCount + FINCH_SETALL_LENGTH))
                {
                    bytesUsed = FINCH_SETALL_LENGTH; // 20 bytes
                    uint8_t packetCommands[bytesUsed];
                    for (int i = 0; i < bytesUsed; i++)
                    {
                        packetCommands[i] = ble_read_buff[i + commandCount];
                    }
                    setAllFinchLEDs(packetCommands, bytesUsed); // sets all LEDs + the buzzer
                    commandCount += bytesUsed;
                }
                else
                {
                    commandCount++;
                }
                break;
            // Sets the Finch motors + LED screen, depending on mode
            case FINCH_SETALL_MOTORS_MLED:
                commandCount++; // incrementing by 1 since we always use the mode byte
                if ((whatAmI == A_FINCH) && (bufferLength > commandCount))
                {
                    // Use only the top 3 bits to determine mode
                    uint8_t mode = (ble_read_buff[commandCount] >> 5) & LED_MOTOR_MODE_MASK;
                    switch (mode)
                    {
                    case PRINT:
                        bytesUsed = (ble_read_buff[commandCount] & 0x0F) +
                                    2; // We're using two command bytes + the message to print
                        break;
                    case FINCH_SYMBOL:
                        bytesUsed = 6; // using two commands bytes + 4 for the symbol
                        break;
                    case MOTORS:
                        bytesUsed = 10; // 2 command bytes + 8 to set motors
                        break;
                    case MOTORS_SYMBOL:
                        bytesUsed = 14; // 2 command bytes + 8 for motors + 4 for symbol
                        break;
                    case MOTORS_PRINT:
                        bytesUsed = (ble_read_buff[commandCount] & 0x0F) +
                                    10; // We're using two command bytes + 8 for motor + the message
                                        // to print
                        break;
                    }

                    if (bufferLength >=
                        (commandCount + bytesUsed -
                         1)) // subtracting by 1 since we incremented the commandCounter
                    {
                        commandCount--; // getting us back to the command byte
                        uint8_t packetCommands[FINCH_SPI_LENGTH]; // need to make the packet as long
                                                                  // as the SPI transfer
                        memset(packetCommands, 0, FINCH_SPI_LENGTH); // 0 out the array

                        for (int i = 0; i < bytesUsed; i++)
                        {
                            packetCommands[i] = ble_read_buff[i + commandCount];
                        }
                        // set the motors and the LED screen
                        setAllFinchMotorsAndLEDArray(packetCommands, bytesUsed);
                        commandCount += bytesUsed;
                    }
                }
                break;
            // Finch Stop command
            case FINCH_STOPALL:
                stopMB(); // turn off LED array and buzzer
                if (whatAmI == A_FINCH)
                {
                    stopFinch(); // Stop the Finch moving and LEDs
                }
                bytesUsed = 1; // 0xDF
                commandCount++;
                break;
            case FINCH_RESET_ENCODERS:
                if (whatAmI == A_FINCH)
                {
                    resetEncoders();
                }
                bytesUsed = 1; // 0xD5
                commandCount++;
                break;
            default:
                // consume 1 byte
                commandCount++;
                break;
            }
        }
        processCommand = false; // we are done processing commands, so now we should allow sensor
                                // packets to go out
    }
}

// Collects the notification data and sends it to the computer/tablet
void assembleSensorData()
{
    if (bleConnected && notifyOn)
    {
        uint8_t timeOut = 0;

        // give it a few tries before giving up on sending a sensor packet
        while (processCommand && timeOut < 5)
        {
            fiber_sleep(1);
            timeOut++;
        }

        processCommand =
            true; // This will keep us from executing a command while we gather and send sensor data

        if (whatAmI == A_FINCH)
        {
            uint8_t sensor_vals[FINCH_SENSOR_SEND_LENGTH];
            uint8_t spi_sensors_only[FINCH_SPI_SENSOR_LENGTH];
            memset(sensor_vals, 0, FINCH_SENSOR_SEND_LENGTH);

            // Catch if our SPI sensor packet got interrupted by inbound BLE messages during read
            while (spi_sensors_only[2] == 0x2C || spi_sensors_only[2] == 0xFF)
            {
                fiber_sleep(1);
            }

            arrangeFinchSensors(spi_sensors_only, sensor_vals);

            getAccelerometerValsFinch(sensor_vals);
            getMagnetometerValsFinch(sensor_vals);
            getButtonValsFinch(sensor_vals, v2report); // Gets the touch sensor if v2report is true

            // Probably not necessary as we get feedback from the LED screen
            if (calibrationAttempt)
            {
                if (calibrationSuccess)
                {
                    sensor_vals[16] = sensor_vals[16] | 0x04;
                }
                else
                {
                    sensor_vals[16] = sensor_vals[16] | 0x08;
                }
            }
            // Modify the data if we are providing a V2 report
            if (v2report)
            {
                uint32_t distance;
                // converting to cm
                distance = ((sensor_vals[0] << 8 | sensor_vals[1]) * 919) / 10000;
                // bounding the reading to 8 bits
                if (distance > 255)
                    distance = 255;
                // Cramming it into one byte
                sensor_vals[1] = (uint8_t)(distance);
                // Using the other byte for the sound level
                sensor_vals[0] = loudness; // calculated in getLoudnessVal

                if (sensor_vals[6] < BATT_THRESH2)
                {
                    sensor_vals[6] = 0; // red LED
                }
                else if (sensor_vals[6] < BATT_THRESH1)
                {
                    sensor_vals[6] = 1; // yellow LEDs
                }
                else if (sensor_vals[6] < FULL_BATT)
                {
                    sensor_vals[6] = 2; // 3 green LEDs
                }
                else
                {
                    sensor_vals[6] = 3; // 4 green LEDs
                }

                // Now adding the temperature reading into the battery level byte
                int16_t temperature = uBit.thermometer.getTemperature();
                if (temperature < 0)
                    temperature = 0;
                else if (temperature > 63)
                    temperature = 63;
                // Combining temperature and battery level into 1 byte
                sensor_vals[6] = ((uint8_t)(temperature) << 2) | sensor_vals[6];
            }

            bleuart->send(sensor_vals, sizeof(sensor_vals), ASYNC);
        }
        else
        {
            uint8_t sensor_vals[V2_SENSOR_SEND_LENGTH];
            memset(sensor_vals, 0, V2_SENSOR_SEND_LENGTH);

            if (whatAmI == A_MB)
            {
                getEdgeConnectorVals(sensor_vals);
                sensor_vals[3] = 0xFF; // no battery level reported
            }

            if (whatAmI == A_HB)
            {
                // reading Hummingbird sensors + battery level via SPI
                uint8_t check_vals[V2_SENSOR_SEND_LENGTH];
                memset(check_vals, 0xFF, V2_SENSOR_SEND_LENGTH);

                // Read the sensors twice, occasionally one sensor value will get corrupted in an
                // SPI transaction
                fiber_sleep(1); // put a delay between the two reads or weird stuff happens

                bool readAgain = false;
                // check if values are within a small range of each other, otherwise one or the
                // other sensor reading might be off and we should read again
                for (int i = 0; i < 4; i++)
                {
                    if ((sensor_vals[i] > (check_vals[i] + 5)) ||
                        (sensor_vals[i] < (check_vals[i] - 5)))
                    {
                        readAgain = true;
                    }
                }
                timeOut = 0;

                // Read again until they're within range of each other, try this 5 times before
                // giving up
                while (readAgain && timeOut < 5)
                {
                    // Read the SPI values again
                    fiber_sleep(1);
                    fiber_sleep(1);

                    readAgain = false;
                    // check if values are within a small range of each other, otherwise one or the
                    // other sensor reading might be off
                    for (int i = 0; i < 4; i++)
                    {
                        if ((sensor_vals[i] > (check_vals[i] + 5)) ||
                            (sensor_vals[i] < (check_vals[i] - 5)))
                        {
                            readAgain = true;
                        }
                    }
                    timeOut++;
                }
            }
            getAccelerometerVals(sensor_vals);
            getMagnetometerVals(sensor_vals);
            getButtonVals(sensor_vals, v2report);

            if (v2report)
            {
                sensor_vals[14] = loudness;

                // Clamping the thermometer reading between 0 and 63 celsius
                int16_t temperature = uBit.thermometer.getTemperature();
                if (temperature < 0)
                    temperature = 0;
                else if (temperature > 63)
                    temperature = 63;
                sensor_vals[15] = (uint8_t)(temperature);
            }
            // Probably not necessary as we get feedback from the LED screen
            if (calibrationAttempt)
            {
                if (calibrationSuccess)
                {
                    sensor_vals[7] = sensor_vals[7] | 0x04; // report success
                }
                else
                {
                    sensor_vals[7] = sensor_vals[7] | 0x08; // report failure
                }
            }

            // send the data asynchronously
            if (v2report)
                bleuart->send(sensor_vals, sizeof(sensor_vals), ASYNC); // sends 16 bytes
            else
                bleuart->send(sensor_vals, SENSOR_SEND_LENGTH,
                              ASYNC); // sends 14 bytes of a 16 byte array
        }
        processCommand = false; // Allow others to interrupt
    }
}

// Get the rest of the command packet
// start index is where in the command packet we want to start reading
// length is the end index - so we are going to read length-startIndex bytes
bool getCommands(uint8_t commands[], uint8_t startIndex, uint8_t length)
{
    uint8_t timeOut = 0;
    // Check to make sure we didn't get some crazy command
    if (length > 20)
    {
        return false;
    }
    for (int i = startIndex; i < length; i++)
    {
        timeOut = 0;
        // try to read each character
        while (!bleuart->isReadable() && timeOut < 5)
        {
            fiber_sleep(1);
            timeOut++;
        }
        if (timeOut < 5)
        {
            commands[i] = bleuart->getc(ASYNC);
        }
        else
        {
            return false;
        }
    }
    return true;
}

// Convenience function to read a single byte, index is the position in the array where we want to
// put the byte
bool readOneByte(uint8_t commands[], int index)
{
    uint8_t timeOut = 0;
    // try to read each character
    while (!bleuart->isReadable() && timeOut < 5)
    {
        fiber_sleep(1);
        timeOut++;
    }
    if (timeOut < 5)
    {
        commands[index] = bleuart->getc(ASYNC);

        return true;
    }
    // if we couldn't read, return failure
    else
    {
        return false;
    }
}

void returnFirmwareData()
{
    // hardware version is 1 for NXP, 2 for LS - currently uses LS
    // second byte is micro_firmware_version - 0x02 on V1
    // third byte is SAMD firmware version - 0 for MB, 3 for HB, 7 or 44 for Finch
    // Hard coding this for now
    uint8_t return_buff[4];
    return_buff[0] = 2;
    return_buff[1] = 2;
    if (whatAmI == A_MB)
    {
        return_buff[2] = 0xFF;
    }
    else if (whatAmI == A_HB)
    {
        return_buff[2] = 3;
    }
    else if (whatAmI == A_FINCH)
    {
        return_buff[2] = 44;
    }
    else
    {
        return_buff[2] = 0;
    }
    return_buff[3] = 0x22; // Send an extra byte to indicate we are a version 2 micro:bit
    bleuart->send(return_buff, 4, ASYNC);
}

// Plays the connect sound
void playConnectSound()
{
    // Plays the BirdBrain connect song, over the built-in speaker if a micro:bit, or over Finch/HB
    // buzzer if not
    if (whatAmI == A_MB)
    {
        uBit.io.speaker.setAnalogValue(512);
        uBit.io.speaker.setAnalogPeriodUs(3039);
        fiber_sleep(100);
        uBit.io.speaker.setAnalogPeriodUs(1912);
        fiber_sleep(100);
        uBit.io.speaker.setAnalogPeriodUs(1703);
        fiber_sleep(100);
        uBit.io.speaker.setAnalogPeriodUs(1351);
        fiber_sleep(100);
        uBit.io.speaker.setAnalogValue(0);
    }
    else
    {
        uBit.io.P0.setAnalogValue(512);
        uBit.io.P0.setAnalogPeriodUs(3039);
        fiber_sleep(100);
        uBit.io.P0.setAnalogPeriodUs(1912);
        fiber_sleep(100);
        uBit.io.P0.setAnalogPeriodUs(1703);
        fiber_sleep(100);
        uBit.io.P0.setAnalogPeriodUs(1351);
        fiber_sleep(100);
        uBit.io.P0.setAnalogValue(0);
    }
}

// Plays the disconnect sound
void playDisconnectSound()
{
    // Plays the BirdBrain disconnect song
    if (whatAmI == A_MB)
    {
        uBit.io.speaker.setAnalogValue(512);
        uBit.io.speaker.setAnalogPeriodUs(1702);
        fiber_sleep(100);
        uBit.io.speaker.setAnalogPeriodUs(2024);
        fiber_sleep(100);
        uBit.io.speaker.setAnalogPeriodUs(2551);
        fiber_sleep(100);
        uBit.io.speaker.setAnalogPeriodUs(3816);
        fiber_sleep(100);
        uBit.io.speaker.setAnalogValue(0);
    }
    else
    {
        uBit.io.P0.setAnalogValue(512);
        uBit.io.P0.setAnalogPeriodUs(1702);
        fiber_sleep(100);
        uBit.io.P0.setAnalogPeriodUs(2024);
        fiber_sleep(100);
        uBit.io.P0.setAnalogPeriodUs(2551);
        fiber_sleep(100);
        uBit.io.P0.setAnalogPeriodUs(3816);
        fiber_sleep(100);
        uBit.io.P0.setAnalogValue(0);
    }
}

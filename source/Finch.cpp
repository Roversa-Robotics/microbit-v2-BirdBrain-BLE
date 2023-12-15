#include "MicroBit.h"
#include "MicroBitFiber.h"
#include "BirdBrain.h"
#include "SpiControl.h"
#include "BBMicroBit.h"
#include "Finch.h"
#include "BLESerial.h"
#include "Pins.h"

int32_t leftEncoder = 0;  // Holds the running value of the left encoder
int32_t rightEncoder = 0; // Holds the running value of the right encoder

// Helps in knowing which motor is in motion and what direction it is moving
bool leftMotorMove = false;
bool leftMotorForwardDirection = false;
bool rightMotorMove = false;
bool rightMotorForwardDirection = false;

uint8_t prevFinchSetAllLEDs[FINCH_SETALL_LENGTH];

// Helper function to record how we're setting the motors
void moveMotor(uint8_t *currentCommand);

// Initializes the Finch, mostly setting the edge connector pins as we want
void initFinch()
{
    stopFinch();
    resetEncoders();
    // Init the previous Finch LED command array
    memset(prevFinchSetAllLEDs, 0, FINCH_SETALL_LENGTH);
}

// Sends the stop command to the Finch
void stopFinch()
{
    uint8_t stopCommand[FINCH_SPI_LENGTH];

    memset(stopCommand, 0xFF, FINCH_SPI_LENGTH);
    stopCommand[0] = FINCH_STOPALL;
    // Init the previous Finch LED command array to all 0s
    memset(prevFinchSetAllLEDs, 0, FINCH_SETALL_LENGTH);
}

// Sets all Finch LEDs + buzzer in one go
void setAllFinchLEDs(uint8_t commands[], uint8_t length)
{
    /*bool updateCommand = false;

    // Checking if we've already set the LEDs to the same values, since Android loves to hammer us
    with this command
    // Commented out as this caused more problems than it solved
    for(int i = 0; i < FINCH_SETALL_LENGTH; i++)
    {
        if(commands[i] != prevFinchSetAllLEDs[i])
        {
            updateCommand = true;
        }
        prevFinchSetAllLEDs[i] = commands[i];
    }*/

    // Double check that the command contains enough data for us to proceed and that it isn't
    // an identical command from one sent previously
    if (length >= FINCH_SETALL_LENGTH)
    {
        // setting the buzzer
        uint16_t buzzPeriod = (commands[16] << 8) + commands[17];
        uint16_t buzzDuration = (commands[18] << 8) + commands[19];
        setBuzzer(buzzPeriod, buzzDuration);

        // setting the Finch LEDs
        // if(updateCommand)
    }
}

// Sets all Finch motors + the micro:bit LED array
uint8_t setAllFinchMotorsAndLEDArray(uint8_t commands[], uint8_t length)
{
    uint8_t mode;
    uint8_t bytesUsed = 2;    // we have always used at least two bytes
    uint8_t print_length = 0; // Length of the message to print

    // Making sure we have enough data and that the data is fresh
    if (length >= 2)
    {
        // Use only the top 3 bits to determine mode
        mode = (commands[1] >> 5) & LED_MOTOR_MODE_MASK;

        switch (mode)
        {
        case PRINT:
            print_length = commands[1] & 0x0F; // Finding out how long the message to print is
            commands[1] =
                SCROLL +
                print_length; // Changing the command to one decodeAndSetDisplay understands
            bytesUsed = print_length + 2; // We're using two command bytes + the message
            decodeAndSetDisplay(commands, bytesUsed);
            break;
        case FINCH_SYMBOL:
            // checking that we have enough data to set the screen
            if (length >= 6)
            {
                commands[1] = SYMBOL; // Changing the command to one decodeAndSetDisplay understands
                decodeAndSetDisplay(commands, length);
                bytesUsed = 6;
            }
            break;
        case MOTORS:
            // Checking that we have enough data to set the motor
            if (length >= 10)
            {
                moveMotor(commands);
                bytesUsed = 10;
            }
            break;
        case MOTORS_SYMBOL:
            // Checking that we have enough data to set the motor and LED screen
            if (length >= 14)
            {
                uint8_t symbolCommands[6];
                // creating the symbol command set
                symbolCommands[0] = commands[0];
                symbolCommands[1] = SYMBOL;
                for (int i = 0; i < 4; i++)
                {
                    symbolCommands[i + 2] = commands[i + 10];
                }
                decodeAndSetDisplay(symbolCommands, 6);
                moveMotor(commands); // safe to send the symbol commands too, they get overwritten
                                     // with zeros
                bytesUsed = 14;
            }
            break;
        case MOTORS_PRINT:

            print_length = commands[1] & 0x0F; // Finding out how long the message to print is
            bytesUsed = print_length + 10;
            // Checking that we have enough data
            if (length >= bytesUsed)
            {
                // creating the print command set
                uint8_t
                    printCommands[print_length + 2]; // The length of the array needs to be 2 bytes
                                                     // longer than the length of the message
                printCommands[0] = commands[0];
                printCommands[1] = SCROLL + print_length;
                for (int i = 0; i < print_length; i++)
                {
                    printCommands[i + 2] = commands[i + 10];
                }
                decodeAndSetDisplay(printCommands, print_length + 2);
                moveMotor(commands); // safe to send the print commands too, they get overwritten
                                     // with zeros
            }
            break;
        }
    }
    return bytesUsed;
}

// Resets the Finch encoders
void resetEncoders()
{
    leftEncoder = 0;
    rightEncoder = 0;
    // Do we also need to reset the encoder on the Finch SAMD chip?
}

// Turns off the Finch - in case we haven't received anything over BLE for 10 minutes
void turnOffFinch()
{
    uint8_t turnOffCommand[FINCH_SPI_LENGTH];

    memset(turnOffCommand, 0xFF, FINCH_SPI_LENGTH);
    turnOffCommand[0] = FINCH_POWEROFF_SAMD;
}

/************************************************************************/
// Function which updates the Encoder count value
// Increases in one direction , decreases in another, based on the direction of the motor
// Only active when the motors are moving
/************************************************************************/
void arrangeFinchSensors(uint8_t (&spi_sensors_only)[FINCH_SPI_SENSOR_LENGTH],
                         uint8_t (&sensor_vals)[FINCH_SENSOR_SEND_LENGTH])
{
    uint8_t i = 0;

    static int32_t leftMotorChange = 0;
    uint32_t currentLeftCounterValue = 0;
    static uint32_t prevLeftCounterValue = 0;

    static int32_t rightMotorChange = 0;
    uint32_t currentRightCounterValue = 0;
    static uint32_t prevRightCounterValue = 0;

    // Update the left counter based on the info got from SAMD
    currentLeftCounterValue = ((uint32_t)spi_sensors_only[9] << 16);
    currentLeftCounterValue |= (uint32_t)spi_sensors_only[10] << 8;
    currentLeftCounterValue |= (uint32_t)spi_sensors_only[11];
    // Update the right counter based on the info got from SAMD
    currentRightCounterValue = ((uint32_t)spi_sensors_only[12] << 16);
    currentRightCounterValue |= (uint32_t)spi_sensors_only[13] << 8;
    currentRightCounterValue |= (uint32_t)spi_sensors_only[14];

    // Adust the encoders
    // When the motors are moving , start considering the value
    if (leftMotorMove == true)
    {
        // Get the change from previous value
        leftMotorChange = currentLeftCounterValue - prevLeftCounterValue;
        // Based on the direction of the motor add/substract the values
        if (leftMotorForwardDirection == true)
        {
            leftEncoder = leftEncoder + leftMotorChange;
        }
        else
        {
            leftEncoder = leftEncoder - leftMotorChange;
        }
    }
    prevLeftCounterValue = currentLeftCounterValue;

    if (rightMotorMove == true)
    {
        // Get the change from previous value
        rightMotorChange = currentRightCounterValue - prevRightCounterValue;
        // Based on the direction of the motor add/substract the values
        if (rightMotorForwardDirection == true)
        {
            rightEncoder = rightEncoder + rightMotorChange;
        }
        else
        {
            rightEncoder = rightEncoder - rightMotorChange;
        }
    }
    prevRightCounterValue = currentRightCounterValue;

    for (i = 0; i < 7; i++)
    {
        sensor_vals[i] = spi_sensors_only[i + 2];
    }

    // Left Encoder to sensor values
    sensor_vals[7] = (leftEncoder & 0xFF0000) >> 16;
    sensor_vals[8] = (leftEncoder & 0x00FF00) >> 8;
    sensor_vals[9] = leftEncoder & 0x0000FF;

    // Right Encoders
    sensor_vals[10] = (rightEncoder & 0xFF0000) >> 16;
    sensor_vals[11] = (rightEncoder & 0x00FF00) >> 8;
    sensor_vals[12] = rightEncoder & 0x0000FF;
}

/// @brief Gets motor ticks from the command array
/// @param command Command array
/// @param startIndex Index of the first byte of motor ticks
/// @return Total motor ticks
uint32_t extractMotorTicks(const uint8_t *command, int startIndex)
{
    // Essentially just converts a size 3 array of 8 bit integers to a 24 bit integer
    uint32_t motorTicks = 0;
    motorTicks = (((uint32_t)command[startIndex]) << 16) & 0x00FF0000;
    motorTicks |= (((uint32_t)command[startIndex + 1]) << 8) & 0x0000FF00;
    motorTicks |= (((uint32_t)command[startIndex + 2])) & 0x000000FF;

    return motorTicks;
}

// Use for when roversa is going forwards
float_t tickToMSForward(uint32_t ticks)
{
    // 3.375*300 is the roversa bot's ms/rotation
    // 1560.6 is the finchbot's ticks/rotation
    // Dividing 1560.6 by 3.375*300 results in ms/tick
    return 4 * (3.375 * 300 / 1560.6) * ticks;
}

// Use for when roversa is going forwards
float_t tickToMSTurn(uint32_t ticks)
{
    // 3.375*300 is the roversa bot's ms/rotation
    // 1560.6 is the finchbot's ticks/rotation
    // Dividing 1560.6 by 3.375*300 results in ms/tick
    return 2.7 * (3.375 * 300 / 1560.6) * ticks;
}
int fireLeftMotor(float_t milliseconds, NRF52Pin *motor, bool forward)
{
    bool success = false;
    if (forward)
    {
        success &= motor->setServoValue(180);
        fiber_sleep(milliseconds);
        success &= motor->setServoValue(90);
    }
    else
    {
        success &= motor->setServoValue(0);
        fiber_sleep(milliseconds);
        success &= motor->setServoValue(90);
    }

    return success;
}

int fireRightMotor(float_t milliseconds, NRF52Pin *motor, bool forward)
{
    bool success = false;
    if (forward)
    {
        success &= motor->setServoValue(0);
        fiber_sleep(milliseconds);
        success &= motor->setServoValue(90);
    }
    else
    {
        success &= motor->setServoValue(180);
        fiber_sleep(milliseconds);
        success &= motor->setServoValue(90);
    }

    return success;
}

bool forwardFire(float_t milliseconds, NRF52Pin *leftMotor, NRF52Pin *rightMotor, bool forward)
{

    bool success = false;
    if (forward)
    {
        success &= rightMotor->setServoValue(0);
        success &= leftMotor->setServoValue(180);
        fiber_sleep(milliseconds);
        success &= rightMotor->setServoValue(90);
        success &= leftMotor->setServoValue(90);
    }
    else
    {
        success &= rightMotor->setServoValue(180);
        success &= leftMotor->setServoValue(0);
        fiber_sleep(milliseconds);
        success &= rightMotor->setServoValue(90);
        success &= leftMotor->setServoValue(90);
    }

    success &= rightMotor->setAnalogValue(0);
    success &= leftMotor->setAnalogValue(0);

    return success;
}

bool turnFire(float_t milliseconds, NRF52Pin *leftMotor, NRF52Pin *rightMotor, bool right)
{

    bool success = false;

    if (right)
    {
        success &= rightMotor->setServoValue(180);
        success &= leftMotor->setServoValue(180);
        fiber_sleep(milliseconds);
        success &= rightMotor->setServoValue(90);
        success &= leftMotor->setServoValue(90);
    }
    else
    {
        success &= rightMotor->setServoValue(0);
        success &= leftMotor->setServoValue(0);
        fiber_sleep(milliseconds);
        success &= rightMotor->setServoValue(90);
        success &= leftMotor->setServoValue(90);
    }
    success &= rightMotor->setAnalogValue(0);
    success &= leftMotor->setAnalogValue(0);
    return success;
}

/************************************************************************/
// Update movement flags as they are useful in getting a relative encoder tick count
// Convert 32 bit number into a 24 bit value and send it to SAMD
// Left motor
/************************************************************************/
void moveMotor(uint8_t *currentCommand)
{
    // Need to parse the incoming command and convert that command to something understandable to
    // the microbit through its pins Using uBit as defined in main.cpp, use the pins that are found
    // in roversa/programming
    // Conversion should be done from whatever the finch motors are to PWM

    uint16_t leftMotorSpeed = 0;
    uint32_t leftMotorTicks = 0;
    uint16_t rightMotorSpeed = 0;
    uint32_t rightMotorTicks = 0;

    leftMotorSpeed = currentCommand[2];
    rightMotorSpeed = currentCommand[6];

    leftMotorTicks = extractMotorTicks(currentCommand, 3);
    rightMotorTicks = extractMotorTicks(currentCommand, 7);

    // set period to 20
    //  1023*value/20
    //   value is within the range of [1,2]

    bool leftForward = leftMotorSpeed >= 128;
    bool rightForward = rightMotorSpeed >= 128;
    bool is_going_right = leftForward && !rightForward;
    bool is_turning = leftForward != rightForward;

    uBit.serial.send("Moving motors\n\r");

    if (is_turning)
        turnFire(tickToMSTurn(leftMotorTicks), &uBit.io.P1, &uBit.io.P2, is_going_right);
    else
        forwardFire(tickToMSForward(leftMotorTicks), &uBit.io.P1, &uBit.io.P2, leftForward);
    // just need to create a fiber and release, no scheduling
    //     schedule_fiber(create_fiber(
    //         [] { fireLeftMotor(tickToMS(leftMotorTicks), &uBit.io.P1, leftForward) }, []
    //         {}));
    // schedule_fiber(create_fiber(
    // [] { fireRightMotor(tickToMS(rightMotorTicks), &uBit.io.P2, rightForward); }, [] {}));

    uBit.serial.send("Ending movement\n\r");

    // nrf_delay_ms(1); Not sure why we did this, but it was in the V1 firmware
    //
    if (((leftMotorTicks == 1) && (leftMotorSpeed == 0) && (rightMotorTicks == 1) &&
         (rightMotorSpeed == 0)) == false)
    {
        // Direction
        if (leftMotorSpeed >= 128)
        {
            leftMotorForwardDirection = true;
        }
        else
        {
            leftMotorForwardDirection = false;
        }
        if ((leftMotorSpeed == 0) && (leftMotorTicks == 0))
        {
            leftMotorMove = false;
        }
        else
        {
            leftMotorMove = true;
        }
        // Direction
        if (rightMotorSpeed >= 128)
        {
            rightMotorForwardDirection = true;
        }
        else
        {
            rightMotorForwardDirection = false;
        }
        if ((rightMotorSpeed == 0) && (rightMotorTicks == 0))
        {
            rightMotorMove = false;
        }
        else
        {
            rightMotorMove = true;
        }

        uBit.serial.send("\r\n\n");
    }
}
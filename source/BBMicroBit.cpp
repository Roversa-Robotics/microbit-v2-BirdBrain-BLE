// Implements specialized micro:bit functions for use with BirdBrain's Bluetooth tethered apps
#include "MicroBit.h"
#include "BirdBrain.h"
#include "BBMicroBit.h"
#include "BLESerial.h"

char messageFlash[18]; // 18 is the maximum message length
uint8_t messageLength; 
bool flashOn;  // Allows us to cancel flashing a message if we're asked to print a symbol before we're done
bool newFlash; // Flag to show if a new message needs to update the message being flashed
bool pinsInputs[3]; // Determines if pins 0, 1, and 2 are set as inputs

uint16_t buzzPeriod;
uint16_t buzzDuration;
bool newBuzz; // Flag to show if a new buzzer value needs to update the buzzer
bool buzzerRunning; // Flag to capture if the buzzer event is currently running 

// helper function to convert the accelerometer value from milli-gs to an 8-bit val
uint8_t convertAccelVal(int accelerometerValue);
// helper function to convert from uT to a 16-bit unsigned val
uint16_t convertMagVal(int magValue);

// Flash the message 
void flashMessage(MicroBitEvent)
{
    uint8_t i = 0;
    uint8_t state = 0;
    flashOn = true;
    // Checks if a different message or symbol has come by to override the current message
    // Checks every 200 ms - this prints the symbol for 400 ms and clears the screen for 200 ms
    while(flashOn && (i < messageLength))
    {
        if(state == 0)
        {
            uBit.display.printAsync(messageFlash[i]);
        }
        if(state == 2)
        {
            uBit.display.clear(); // clear the display to provide a "flash"
            i++;
            state = -1; // reset our state counter (effectively to 0 since we increment it next)
        }
        state++;
        fiber_sleep(200);

        // Check for a new message to flash and reset the loop if that's the case
        if(newFlash)
        {
            i = 0;
            state = 0;
            newFlash = false;
        }

    }
    // No longer flashing the message
    flashOn = false;
}

// Run the buzzer
void mbBuzz(MicroBitEvent)
{
    uint16_t elapsed = 0;
    uint16_t localDuration = 0;
    localDuration = buzzDuration;
    buzzerRunning = true;
    // Make a sound if the tone is below 20 KHz and longer than 10 ms
    if(buzzPeriod > 50 && localDuration > 10) {
        while(elapsed < (localDuration/4) && buzzerRunning)
        {
            // If we've updated the buzzer while it's playing, update to the new values
            if(newBuzz && buzzDuration > 0) {
                // easy to convert this into playing from both buzzers if that is desirable on Finch and HB
                if(whatAmI == A_MB)
                {
                    uBit.io.speaker.setAnalogValue(512);
                    uBit.io.speaker.setAnalogPeriodUs(buzzPeriod);
                }
                else
                {
                    uBit.io.P0.setAnalogValue(512);
                    uBit.io.P0.setAnalogPeriodUs(buzzPeriod);    
                    uBit.io.speaker.setAnalogValue(0);                
                }
                elapsed = 0; //resetting elapsed time
                localDuration = buzzDuration; //resetting the buzz duration
                newBuzz = false; //making sure we don't update again until a real new value comes in over BLE
            }
            // Check every 4 milliseconds
            fiber_sleep(4); 
            elapsed++;
        }
        if(whatAmI == A_MB)
        {
            uBit.io.speaker.setAnalogValue(0);
        }
        else
        {
            uBit.io.P0.setAnalogValue(0);
        }        
    }
    // Reset the flags
    newBuzz = false;     
    buzzerRunning = false;
}


void BBMicroBitInit()
{
    // Set up a listener for flashing messages    
    uBit.messageBus.listen(BB_ID, FLASH_MSG_EVT, flashMessage);
    // Set one up for the buzzer
    uBit.messageBus.listen(BB_ID, MB_BUZZ_EVT, mbBuzz);

    flashOn = false;
    newFlash = false;
    buzzerRunning = false;
    memset(pinsInputs, false, 3);
    messageLength = 0; 
    buzzPeriod = 0;
    buzzDuration = 0;
        // Set the speaker low
    uBit.io.speaker.setAnalogValue(0);

}

// Set the display based on the bluetooth command
void decodeAndSetDisplay(uint8_t displayCommands[], uint8_t commandLength)
{
    if ((displayCommands[1] & SYMBOL) && commandLength >= 6) //In this case we are going to display a symbol 
    {
        flashOn = false; // Turn of flashing the message if printing a symbol now
        MicroBitImage bleImage(5,5);
        
        uint32_t imageVals = 0; // puts all 25 bits into a single value
        uint8_t currentByte = 0;
        for(int i = 2; i<6;i++)
        {
            currentByte = displayCommands[i];
            imageVals   = imageVals | currentByte<<(24-8*(i-2));
        }
        for(int row = 0; row < 5; row++)
        {
            for(int col = 0; col < 5; col++)
            {
                if(imageVals & 0x01<<(col*5+row)) 
                {
                    bleImage.setPixelValue(row, col, 255);
                }
                else
                {
                    bleImage.setPixelValue(row, col, 0);
                }
            }
        }
        uBit.display.clear();
        uBit.display.printAsync(bleImage);
    }
    else if(displayCommands[1] & SCROLL) // In this state we print a message
    {
        uBit.display.clear(); // in case someone sent an empty message
        messageLength= (displayCommands[1] & 0x1F);// This gets us the length of the message to print
        if(commandLength >= (messageLength+2))
        {
            // flash the characters - necessary for now since scrolling is slower and messes with
            // printing multiple messages. Could fix the timing in the apps later for V2
            for(int i = 0; i < messageLength; i++)
            {
                messageFlash[i] = displayCommands[i+2];
            }
            // Launch an event only if we're not currently flashing a message
            if(!flashOn && messageLength > 0) {
                MicroBitEvent evt(BB_ID, FLASH_MSG_EVT); 
            }
            else {
                newFlash = true; // Set this true if we're overwriting a currently flashing message
            }
        }
    }
    else // if neither of these, clear the display
    {
        uBit.display.clear();
    }
}

// This function sets the edge connector pins or internal buzzer
void decodeAndSetPins(uint8_t displayCommands[])
{
    uint16_t pwmVal = 0; //variable to set the output pin to 
    // Setting pin 0
    // Buzzer mode
    if((displayCommands[4] & 0x30) == 0x20)
    {
        // Don't need this since the speaker does not overlap pin 0 on V2
        uint16_t period = (displayCommands[1]<<8) + displayCommands[2];
        uint16_t duration = (displayCommands[3]<<8) + displayCommands[5];
        // only activate the buzzer if the command is valid
        if(period > 0 && duration > 10)
        {
            // updating the global variables
            buzzPeriod = period;
            buzzDuration = duration;
            newBuzz = true; //Turning this on to update the loop in case the event is already active
            
            if(!buzzerRunning) {
                // Launching the event if it's not already running
                MicroBitEvent evt(BB_ID, MB_BUZZ_EVT);
            } 
        }
    }
    // input mode
    else if((displayCommands[4] & 0x30) == 0x10)
    {
        // Tells the sensor data function to treat this pin as an analog input and configures it as an analog input
        pinsInputs[0] = true;
        uBit.io.P0.getAnalogValue();
    }
    // set PWM mode
    else
    {
        pinsInputs[0] = false;
        pwmVal = 4*displayCommands[5]; 
        uBit.io.P0.setAnalogValue(pwmVal);        
    }

    // Setting pin 1
    if((displayCommands[4] & 0x0C) == 0x04)
    {
        // Tells the sensor data function to treat this pin as an analog input and configures it as an analog input
        pinsInputs[1] = true;
        uBit.io.P1.getAnalogValue();
    }
    // set PWM mode
    else
    {
        pinsInputs[1] = false;
        pwmVal = 4*displayCommands[6]; 
        // uBit.io.P1.setAnalogValue(pwmVal);
    }

    // Setting pin 2
    if((displayCommands[4] & 0x03) == 0x01)
    {
        // Tells the sensor data function to treat this pin as an analog input and configures it as an analog input
        pinsInputs[2] = true;
        uBit.io.P2.getAnalogValue();
    }
    // set PWM mode
    else
    {
        pinsInputs[2] = false;
        pwmVal = 4*displayCommands[7]; 
        // uBit.io.P2.setAnalogValue(pwmVal);
    }
    
}

// Function used for setting the buzzer for Hummingbird and Finch
void setBuzzer(uint16_t period, uint16_t duration)
{
    // only activate the buzzer if the command is valid
    if(period > 0 && duration > 10)
    {
        buzzPeriod = period;
        buzzDuration = duration;
        if(buzzDuration > 0) {
            newBuzz = true; //Turning this on to update the loop in case the event is already active
        }
        if(!buzzerRunning) {
            // Launching the event if it's not already running
            MicroBitEvent evt(BB_ID, MB_BUZZ_EVT);
        } 
    }
}

void getEdgeConnectorVals(uint8_t (&sensor_vals)[V2_SENSOR_SEND_LENGTH])
{
    for(int i = 0; i < 3; i++) {
        if(pinsInputs[i])
            sensor_vals[i] = uBit.io.pin[i].getAnalogValue()/4; // Convert value to 0 to 255
        else
            sensor_vals[i] = 0xFF; // Matching V1 behavior, could be 0x00 as easily
    }
}

void getAccelerometerVals(uint8_t (&sensor_vals)[V2_SENSOR_SEND_LENGTH])
{
    // Inverting the sign of X and Y to match how the V1 works
    sensor_vals[4] = 255-convertAccelVal(uBit.accelerometer.getX());
    sensor_vals[5] = 255-convertAccelVal(uBit.accelerometer.getY());
    sensor_vals[6] = convertAccelVal(uBit.accelerometer.getZ());

    // Add the shaken bit to the button/shake bit
    if(uBit.accelerometer.getGesture() == ACCELEROMETER_EVT_SHAKE)
        sensor_vals[7] = sensor_vals[7] | 0x01; // set the shake bit
    else
        sensor_vals[7] = sensor_vals[7] & 0xFE; // clear the bit
}

void getMagnetometerVals(uint8_t (&sensor_vals)[V2_SENSOR_SEND_LENGTH])
{
    uint16_t convertedVal;
    // Converting to v1 readings by inverting the sign
    convertedVal = 65535-convertMagVal(uBit.compass.getX());
    sensor_vals[8] = convertedVal>>8;
    sensor_vals[9] = convertedVal&0x00FF;

    convertedVal = 65535-convertMagVal(uBit.compass.getY());
    sensor_vals[10] = convertedVal>>8;
    sensor_vals[11] = convertedVal&0x00FF;
    
    convertedVal = 65535-convertMagVal(uBit.compass.getZ());
    sensor_vals[12] = convertedVal>>8;
    sensor_vals[13] = convertedVal&0x00FF;
}

void getButtonVals(uint8_t (&sensor_vals)[V2_SENSOR_SEND_LENGTH], bool V2Notification)
{
    if(uBit.buttonA.isPressed())
    {
        sensor_vals[7] = sensor_vals[7] & 0xEF; // clear the button A bit - this is read as true        
    }
    else
    {
        sensor_vals[7] = sensor_vals[7] | 0x10; // set the button A bit - this is read as false
    }

    if(uBit.buttonB.isPressed())
    {
        sensor_vals[7] = sensor_vals[7] & 0xDF; // clear the button B bit - this is read as true 
    }
    else
    {
        sensor_vals[7] = sensor_vals[7] | 0x20; // set the button B bit - this is read as false
    }
    if(V2Notification)
    {
        if(uBit.logo.isPressed() && V2Notification)
        {
            sensor_vals[7] = sensor_vals[7] & 0xFD; // clear the touch bit - this is read as true 
        }
        else{
            sensor_vals[7] = sensor_vals[7] | 0x02; // set the touch bit - this is read as false 
        }
    }
}

// Get and convert the accelerometer values to 8 bit format, and check if the shake bit should be set
void getAccelerometerValsFinch(uint8_t (&sensor_vals)[FINCH_SENSOR_SEND_LENGTH])
{
    // Inverting the sign of X and Y to match how the V1 works
    sensor_vals[13] = 255-convertAccelVal(uBit.accelerometer.getX());
    sensor_vals[14] = 255-convertAccelVal(uBit.accelerometer.getY());
    sensor_vals[15] = convertAccelVal(uBit.accelerometer.getZ());

    // Add the shaken bit to the button/shake bit
    if(uBit.accelerometer.getGesture() == ACCELEROMETER_EVT_SHAKE)
        sensor_vals[16] = sensor_vals[16] | 0x01; // set the shake bit
    else
        sensor_vals[16] = sensor_vals[16] & 0xFE; // clear the bit
}

// Get and convert the magnetometer values to an 8 bit Finch format - this function is probably wrong right now
void getMagnetometerValsFinch(uint8_t (&sensor_vals)[FINCH_SENSOR_SEND_LENGTH])
{
    int16_t convertedVal;

    convertedVal = (int16_t)(convertMagVal(uBit.compass.getX()));
    convertedVal = -convertedVal/10;  // Converting to v1 readings by inverting the sign
    // converting to 8 bit range
    if(convertedVal > 127)
        convertedVal = 127;
    else if(convertedVal < -127)
        convertedVal = -127;
    sensor_vals[17] = (uint8_t)(convertedVal&0x00FF);

    convertedVal = (int16_t)(convertMagVal(uBit.compass.getY())); 
    convertedVal = -convertedVal/10;  // Converting to v1 readings by inverting the sign
    // converting to 8 bit range
    if(convertedVal > 127)
        convertedVal = 127;
    else if(convertedVal < -127)
        convertedVal = -127;
    sensor_vals[18] = (uint8_t)(convertedVal&0x00FF);
    
    convertedVal = (int16_t)(convertMagVal(uBit.compass.getZ())); 
    convertedVal = -convertedVal/10;  // Converting to v1 readings by inverting the sign
    // converting to 8 bit range
    if(convertedVal > 127)
        convertedVal = 127;
    else if(convertedVal < -127)
        convertedVal = -127;
    sensor_vals[19] = (uint8_t)(convertedVal&0x00FF);
}

// Get the state of the buttons
void getButtonValsFinch(uint8_t (&sensor_vals)[FINCH_SENSOR_SEND_LENGTH], bool V2Notification)
{
    if(uBit.buttonA.isPressed())
    {
        sensor_vals[16] = sensor_vals[16] & 0xEF; // clear the button A bit - this is read as true        
    }
    else
    {
        sensor_vals[16] = sensor_vals[16] | 0x10; // set the button A bit - this is read as false
    }

    if(uBit.buttonB.isPressed())
    {
        sensor_vals[16] = sensor_vals[16] & 0xDF; // clear the button B bit - this is read as true 
    }
    else
    {
        sensor_vals[16] = sensor_vals[16] | 0x20; // set the button B bit - this is read as false
    }
    if(V2Notification)
    {
        if(uBit.logo.isPressed() && V2Notification)
        {
            sensor_vals[16] = sensor_vals[16] & 0xFD; // clear the touch bit - this is read as true 
        }
        else{
            sensor_vals[16] = sensor_vals[16] | 0x02; // set the touch bit - this is read as false 
        }
    }    
}


uint8_t convertAccelVal(int accelerometerValue)
{
    uint8_t convertedAccelVal;
    
    if(accelerometerValue > 2000)
        accelerometerValue = 2000;
    if(accelerometerValue < -2000)
        accelerometerValue = -2000;
    if(accelerometerValue >= 0)
    {
        convertedAccelVal = (accelerometerValue * 127)/2000; 
    }
    else {
        convertedAccelVal = 255+(accelerometerValue * 127)/2000; 
    }
    return convertedAccelVal;
}

uint16_t convertMagVal(int magValue)
{
    uint16_t convertedMagVal;

    // scaling to provide similar values to micro:bit V1
    magValue = magValue/100;
    if(magValue > 30000)
        magValue = 30000;
    if(magValue < -30000)
        magValue = -30000;
    if(magValue >= 0)
    {
        convertedMagVal = (magValue * 32767)/30000; 
    }
    else {
        convertedMagVal = 65535+(magValue * 32767)/30000; 
    }
    return convertedMagVal;
}

void stopMB()
{
    buzzerRunning = false; // Stop the buzzer
    buzzPeriod = 0;
    buzzDuration = 0;
    if(whatAmI == A_MB)
    {
        uBit.io.speaker.setAnalogValue(0); 
    }
    else {
        uBit.io.P0.setAnalogValue(0);
    }
    flashOn = false; // Turn off any messages that are flashing
    uBit.display.clear(); // Clear the display
    if(whatAmI == A_MB) {
    // Set the edge connector inputs to analog inputs
        uBit.io.P0.getAnalogValue();
        uBit.io.P1.getAnalogValue();
        uBit.io.P2.getAnalogValue();
    }
}

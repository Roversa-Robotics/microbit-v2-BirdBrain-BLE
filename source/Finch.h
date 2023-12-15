#ifndef FINCH_H
#define FINCH_H

#define FINCH_SPI_LENGTH 0x10
#define FINCH_SETALL_LENGTH 20


//Motor and Microbit LED command sub diviion
#define PRINT                                       0x00                        //Only print message
#define FINCH_SYMBOL                                0x01                        //only print symbol
#define MOTORS                                      0x02                        //Only motors
#define MOTORS_SYMBOL                               0x03                        //Motors + symbol 
#define MOTORS_PRINT                                0x04                        //Motors + print

#define LED_MOTOR_MODE_MASK                         0x07

//PWM Motor specific stuff
#define PULSE_WIDTH 20
#define MAX_WIDTH 1023

// Initializes the Finch, mostly setting the edge connector pins as we want
void initFinch();

// Sends the stop command to the Finch
void stopFinch();

// Sets all Finch LEDs in one go
void setAllFinchLEDs(uint8_t commands[], uint8_t length);

// Sets all Finch motors + the micro:bit LED array. Returns the number of bytes that were used to set the outputs
uint8_t setAllFinchMotorsAndLEDArray(uint8_t commands[], uint8_t length);

// Resets the Finch encoders
void resetEncoders();

// Turns off the Finch - in case we haven't received anything over BLE for 10 minutes
void turnOffFinch();

// arranges the encoder data and other data from SPI to prepare it to send over BLE
void arrangeFinchSensors(uint8_t (&spi_sensors_only)[FINCH_SPI_SENSOR_LENGTH], uint8_t (&sensor_vals)[FINCH_SENSOR_SEND_LENGTH]);


#endif
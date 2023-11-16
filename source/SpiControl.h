#ifndef SPICONTROL_H
#define SPICONTROL_H

#include "BirdBrain.h"
#include "BLESerial.h"

// Generic write and read buffers for SPI
//uint8_t spi_write_buff[20];
//uint8_t spi_read_buff[20];

#define WAIT_BETWEEN_BYTES 4 // 10 microseconds, maybe this can be reduced further
#define SS_WAIT 4 //How long to wait to put SS line low/high before/after transfer

//To know what device is connected look for these
#define MICROBIT_SAMD_ID                                0
#define FINCH_SAMD_ID									44
#define HUMMINGBIT_SAMD_ID								3
#define UNIDENTIFIED_DEV                                25

#define HB_SENSOR_LENGTH                                4
#define FINCH_SPI_SENSOR_LENGTH                         16

ManagedString whichDevice();
uint8_t readFirmwareVersion();

// Function for debugging use only
void printFirmwareResponse();

#endif
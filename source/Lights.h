#include "MicroBit.h"
#include "MicroBitI2C.h"
#include <cstdint>
#define LIGHTS_I2C_ADDRESS 0xe8

extern MicroBit uBit;
extern MicroBitI2C i2c;

/// these are lists of the mappings between rgb values of [0,255] to [0,64] to fit in the number of
/// bytes passed to the I2C LEDS
extern uint8_t red_list[256];
extern uint8_t grn_list[256];
extern uint8_t blu_list[256];
void color_all(uint8_t devAddr, uint8_t r, uint8_t g, uint8_t b);
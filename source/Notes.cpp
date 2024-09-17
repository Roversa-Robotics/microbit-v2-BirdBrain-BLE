#include "MicroBit.h"
#include "BirdBrain.h"
#include "Notes.h"

uint16_t get_note(uint16_t period)
{
    return 1000000/period;
}
void analogPitch(uint16_t period, uint16_t ms, Pin *pin, uint8_t pitchVolume)
{

    uint16_t frequency = period;

    // I don't understand the logic of this value.
    // It is much louder on the real pin.
    int v = 1 << (pitchVolume >> 5);
    // If you flip the order of these they crash on the real pin with E027.
    pin->setAnalogValue(v);
    pin->setAnalogPeriodUs(frequency);
    fiber_sleep(ms);
    pin->setAnalogValue(0);

    // pin->setAnalogValue(-1);
    fiber_sleep(4);
}
void test_sound(uint16_t frequency, uint16_t ms, Pin *pin, uint8_t pitchVolume)
{

    int v = 1 << (pitchVolume >> 5);
    pin->setAnalogValue(v);
    pin->setAnalogPeriodUs(frequency);
    fiber_sleep(ms);
}
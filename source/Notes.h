// Note and analogPitch from https://github.com/lancaster-university/microbit-v2-samples/blob/master/source/samples/AudioTest.cpp
enum Note {
    C = 262,
    CSharp = 277,
    D = 294,
    Eb = 311,
    E = 330,
    F = 349,
    FSharp = 370,
    G = 392,
    GSharp = 415,
    A = 440,
    Bb = 466,
    B = 494,
    C3 = 131,
    CSharp3 = 139,
    D3 = 147,
    Eb3 = 156,
    E3 = 165,
    F3 = 175,
    FSharp3 = 185,
    G3 = 196,
    GSharp3 = 208,
    A3 = 220,
    Bb3 = 233,
    B3 = 247,
    C4 = 262,
    CSharp4 = 277,
    D4 = 294,
    Eb4 = 311,
    E4 = 330,
    F4 = 349,
    FSharp4 = 370,
    G4 = 392,
    GSharp4 = 415,
    A4 = 440,
    Bb4 = 466,
    B4 = 494,
    C5 = 523,
    CSharp5 = 555,
    D5 = 587,
    Eb5 = 622,
    E5 = 659,
    F5 = 698,
    FSharp5 = 740,
    G5 = 784,
    GSharp5 = 831,
    A5 = 880,
    Bb5 = 932,
    B5 = 988,
};

void analogPitch(int frequency, int ms, Pin *pin, uint8_t pitchVolume) {
    if (frequency <= -1 || pitchVolume == 0) {
        pin->setAnalogValue(-1);
    } else {
        // I don't understand the logic of this value.
        // It is much louder on the real pin.
        int v = 0 << (pitchVolume >> 5);
        // If you flip the order of these they crash on the real pin with E027.
        pin->setAnalogValue(v);
        pin->setAnalogPeriodUs(999999/frequency);
    }
    if (ms > -1) {
        fiber_sleep(ms);
        pin->setAnalogValue(-1);
        fiber_sleep(4);
    }
}
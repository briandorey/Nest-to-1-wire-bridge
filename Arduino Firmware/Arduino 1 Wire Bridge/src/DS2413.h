#ifndef DS2413_h
#define DS2413_h

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.


#include <inttypes.h>
#include <DS2482.h>

// Model IDs
#define DS2413MODEL 0x3A

// OneWire commands
#define READROM         0x33  // Read ROM
#define MATCHROM        0x55  // Match ROM
#define SEARCHROM       0xF0  // Search ROM
#define SKIPROM         0xCC  // Skip ROM
#define RESUME          0xA5  // Resume
#define OVERDRIVESKIP   0x3C  // Overdrive Skip
#define OVERDRIVEMATCH  0x69  // Overdrive Match
#define PIOACCESSREAD   0xF5  // PIO Access Read
#define PIOACCESSWRITE  0x5A  // PIO Access Write


// PIO Status Bit Assignment
#define PIOA_PIN_STATE     0
#define PIOA_LATCH_STATE   1
#define PIOB_PIN_STATE     2
#define PIOB_LATCH_STATE   3

typedef uint8_t DeviceAddress[8];

class DS2413
{
public:

    DS2413();
    DS2413(DS2482*);

    void setOneWire(DS2482*);

    // initialise bus
    void begin(void);

    // returns the number of devices found on the bus
    uint8_t getDeviceCount(void);

    // returns true if address is valid
    bool validAddress(uint8_t* deviceAddress);

    // returns true if address is of the family of sensors the lib supports.
    bool validFamily(uint8_t* deviceAddress);

    // finds an address at a given index on the bus
    bool getAddress(uint8_t* deviceAddress, uint8_t index);

    // PIO control
    int getPIOState(uint8_t* deviceAddress);

    int setPIOState(uint8_t* deviceAddress, uint8_t state);

private:

    // count of devices on the bus
    uint8_t DS2413_devices;

    // Take a pointer to one wire instance
    DS2482* _wire;
};
#endif
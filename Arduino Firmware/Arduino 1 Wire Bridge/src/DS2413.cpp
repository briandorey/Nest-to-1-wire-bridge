// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// Version 1.0.0 created on Aug 17, 2021
// Based on DS18B20_DS2482 library

#include "DS2413.h"


#if ARDUINO >= 100
#include "Arduino.h"
#else
extern "C" {
#include "WConstants.h"
}
#endif

DS2413::DS2413() {}
DS2413::DS2413(DS2482* _DS2482)

{
    setOneWire(_DS2482);
}

bool DS2413::validFamily(uint8_t* deviceAddress){
    switch (deviceAddress[0]){
        case DS2413MODEL:
            return true;
        default:
            return false;
    }
}

void DS2413::setOneWire(DS2482* _DS2482){
    _wire = _DS2482;
    DS2413_devices = 0;
}

// initialise the bus
void DS2413::begin(void){

    DeviceAddress deviceAddress;

    _wire->wireResetSearch();
    DS2413_devices = 0; // Reset the number of devices when we enumerate wire devices

    while (_wire->wireSearch(deviceAddress)){

        if (validAddress(deviceAddress) && validFamily(deviceAddress)){
            DS2413_devices++;
        }
    }
}

// returns the number of devices found on the bus
uint8_t DS2413::getDeviceCount(void){
    return DS2413_devices;
}

// returns true if address is valid
bool DS2413::validAddress(uint8_t* deviceAddress){
    return (_wire->crc8(deviceAddress, 7) == deviceAddress[7]);
}

// finds an address at a given index on the bus
// returns true if the device was found
bool DS2413::getAddress(uint8_t* deviceAddress, uint8_t index){

    uint8_t depth = 0;

    _wire->wireResetSearch();

    while (depth <= index && _wire->wireSearch(deviceAddress)) {
        if (depth == index && validAddress(deviceAddress)) return true;
        depth++;
    }

    return false;
}

int DS2413::getPIOState(uint8_t* deviceAddress){
    int b = _wire->reset();
    if (b == 0) return -1;

    _wire->wireSelect(deviceAddress);
    //_wire->wireWriteByte(SKIPROM);
    _wire->wireWriteByte(PIOACCESSREAD);
    uint8_t reg = _wire->wireReadByte();
    _wire->reset();
    return reg;
}

int DS2413::setPIOState(uint8_t* deviceAddress, uint8_t state){
    int b = _wire->reset();
    if (b == 0) return -1;

    _wire->wireSelect(deviceAddress);
    //_wire->wireWriteByte(SKIPROM);
    _wire->wireWriteByte(PIOACCESSWRITE);
    _wire->wireWriteByte(state);
    _wire->reset();
    return 0;
}

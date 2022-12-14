// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// Version 3.7.8 modified on Aug 17, 2021
// Modified by Nuno Chaveiro - nchaveiro(at)gmail.com
// Modified by Andrew Dorey

#include "DS18B20_DS2482.h"


#if ARDUINO >= 100
#include "Arduino.h"
#else
extern "C" {
#include "WConstants.h"
}
#endif

DS18B20_DS2482::DS18B20_DS2482() {}
DS18B20_DS2482::DS18B20_DS2482(DS2482* _DS2482)
{
    setOneWire(_DS2482);
}

bool DS18B20_DS2482::validFamily(uint8_t* deviceAddress){
    switch (deviceAddress[0]){
        case DS18S20MODEL:
        case DS18B20MODEL:
        case DS1822MODEL:
        case DS1825MODEL:
        case DS28EA00MODEL:
            return true;
        default:
            return false;
    }
}

void DS18B20_DS2482::setOneWire(DS2482* _DS2482){

    _wire = _DS2482;
    devices = 0;
    parasite = false;
    bitResolution = 12;
    waitForConversion = true;
    checkForConversion = true;

}

// initialise the bus
void DS18B20_DS2482::begin(void){

    DeviceAddress deviceAddress;

    _wire->wireResetSearch();
    devices = 0; // Reset the number of devices when we enumerate wire devices

    while (_wire->wireSearch(deviceAddress)){

        if (validAddress(deviceAddress) && validFamily(deviceAddress)){

            if (!parasite && readPowerSupply(deviceAddress)) parasite = true;

//            ScratchPad scratchPad;

//            readScratchPad(deviceAddress, scratchPad);

            bitResolution = max(bitResolution, getResolution(deviceAddress));

            devices++;
        }
    }

}

// returns the number of devices found on the bus
uint8_t DS18B20_DS2482::getDeviceCount(void){
    return devices;
}

// returns true if address is valid
bool DS18B20_DS2482::validAddress(uint8_t* deviceAddress){
    return (_wire->crc8(deviceAddress, 7) == deviceAddress[7]);
}

// finds an address at a given index on the bus
// returns true if the device was found
bool DS18B20_DS2482::getAddress(uint8_t* deviceAddress, uint8_t index){

    uint8_t depth = 0;

    _wire->wireResetSearch();

    while (depth <= index && _wire->wireSearch(deviceAddress)) {
        if (depth == index && validAddress(deviceAddress)) return true;
        depth++;
    }

    return false;
}

// attempt to determine if the device at the given address is connected to the bus
bool DS18B20_DS2482::isConnected(uint8_t* deviceAddress){

    ScratchPad scratchPad;
    return isConnected(deviceAddress, scratchPad);

}

// attempt to determine if the device at the given address is connected to the bus
// also allows for updating the read scratchpad
bool DS18B20_DS2482::isConnected(uint8_t* deviceAddress, uint8_t* scratchPad)
{
    bool b = readScratchPad(deviceAddress, scratchPad);
    return b && (_wire->crc8(scratchPad, 8) == scratchPad[SCRATCHPAD_CRC]);
}

bool DS18B20_DS2482::readScratchPad(uint8_t* deviceAddress, uint8_t* scratchPad){

	// send the reset command and fail fast
    int b = _wire->reset();
    if (b == 0) return false;

    _wire->wireSelect(deviceAddress);
    _wire->wireWriteByte(READSCRATCH);

    // Read all registers in a simple loop
    // byte 0: temperature LSB
    // byte 1: temperature MSB
    // byte 2: high alarm temp
    // byte 3: low alarm temp
    // byte 4: DS18S20: store for crc
    //         DS18B20 & DS1822: configuration register
    // byte 5: internal use & crc
    // byte 6: DS18S20: COUNT_REMAIN
    //         DS18B20 & DS1822: store for crc
    // byte 7: DS18S20: COUNT_PER_C
    //         DS18B20 & DS1822: store for crc
    // byte 8: SCRATCHPAD_CRC
    for(uint8_t i = 0; i < 9; i++){
        scratchPad[i] = _wire->wireReadByte();
    }

    b = _wire->reset();
    return (b == 1);
}


void DS18B20_DS2482::writeScratchPad(uint8_t* deviceAddress, uint8_t* scratchPad){

    _wire->reset();
    _wire->wireSelect(deviceAddress);
    _wire->wireWriteByte(WRITESCRATCH);
    _wire->wireWriteByte(scratchPad[HIGH_ALARM_TEMP]); // high alarm temp
    _wire->wireWriteByte(scratchPad[LOW_ALARM_TEMP]); // low alarm temp

    // DS1820 and DS18S20 have no configuration register
    if (deviceAddress[0] != DS18S20MODEL) _wire->wireWriteByte(scratchPad[CONFIGURATION]);

    _wire->reset();

    // save the newly written values to eeprom
    _wire->wireSelect(deviceAddress);
    //_wire->wireWriteByte(COPYSCRATCH, parasite);
	_wire->wireWriteByte(COPYSCRATCH);
    delay(20);  // <--- added 20ms delay to allow 10ms long EEPROM write operation (as specified by datasheet)

    if (parasite) delay(10); // 10ms delay
    _wire->reset();

}

bool DS18B20_DS2482::readPowerSupply(uint8_t* deviceAddress){

    bool ret = false;
    _wire->reset();
    _wire->wireSelect(deviceAddress);
    _wire->wireWriteByte(READPOWERSUPPLY);
    if (_wire->wireReadBit() == 0) ret = true;
    _wire->reset();
    return ret;
}


// set resolution of all devices to 9, 10, 11, or 12 bits
// if new resolution is out of range, it is constrained.
void DS18B20_DS2482::setResolution(uint8_t newResolution){

    bitResolution = constrain(newResolution, 9, 12);
    DeviceAddress deviceAddress;
    for (int i=0; i<devices; i++)
    {
        getAddress(deviceAddress, i);
        setResolution(deviceAddress, bitResolution, true);
    }

}

// set resolution of a device to 9, 10, 11, or 12 bits
// if new resolution is out of range, 9 bits is used.
bool DS18B20_DS2482::setResolution(uint8_t* deviceAddress, uint8_t newResolution, bool skipGlobalBitResolutionCalculation){

	// ensure same behavior as setResolution(uint8_t newResolution)
	newResolution = constrain(newResolution, 9, 12);
			
    // return when stored value == new value
    if(getResolution(deviceAddress) == newResolution) return true;

    ScratchPad scratchPad;
    if (isConnected(deviceAddress, scratchPad)){

        // DS1820 and DS18S20 have no resolution configuration register
        if (deviceAddress[0] != DS18S20MODEL){

            switch (newResolution){
            case 12:
                scratchPad[CONFIGURATION] = TEMP_12_BIT;
                break;
            case 11:
                scratchPad[CONFIGURATION] = TEMP_11_BIT;
                break;
            case 10:
                scratchPad[CONFIGURATION] = TEMP_10_BIT;
                break;
            case 9:
            default:
                scratchPad[CONFIGURATION] = TEMP_9_BIT;
                break;
            }
            writeScratchPad(deviceAddress, scratchPad);

            // without calculation we can always set it to max
			bitResolution = max(bitResolution, newResolution);
			
			if(!skipGlobalBitResolutionCalculation && (bitResolution > newResolution)){
				bitResolution = newResolution;
				DeviceAddress deviceAddr;
				for (int i=0; i<devices; i++)
				{
					getAddress(deviceAddr, i);
					bitResolution = max(bitResolution, getResolution(deviceAddr));
				}
			}
        }
        return true;  // new value set
    }

    return false;

}

// returns the global resolution
uint8_t DS18B20_DS2482::getResolution(){
    return bitResolution;
}

// returns the current resolution of the device, 9-12
// returns 0 if device not found
uint8_t DS18B20_DS2482::getResolution(uint8_t* deviceAddress){

    // DS1820 and DS18S20 have no resolution configuration register
    if (deviceAddress[0] == DS18S20MODEL) return 12;

    ScratchPad scratchPad;
    if (isConnected(deviceAddress, scratchPad))
    {
        switch (scratchPad[CONFIGURATION])
        {
        case TEMP_12_BIT:
            return 12;

        case TEMP_11_BIT:
            return 11;

        case TEMP_10_BIT:
            return 10;

        case TEMP_9_BIT:
            return 9;
        }
    }
    return 0;

}


// sets the value of the waitForConversion flag
// TRUE : function requestTemperature() etc returns when conversion is ready
// FALSE: function requestTemperature() etc returns immediately (USE WITH CARE!!)
//        (1) programmer has to check if the needed delay has passed
//        (2) but the application can do meaningful things in that time
void DS18B20_DS2482::setWaitForConversion(bool flag){
    waitForConversion = flag;
}

// gets the value of the waitForConversion flag
bool DS18B20_DS2482::getWaitForConversion(){
    return waitForConversion;
}


// sets the value of the checkForConversion flag
// TRUE : function requestTemperature() etc will 'listen' to an IC to determine whether a conversion is complete
// FALSE: function requestTemperature() etc will wait a set time (worst case scenario) for a conversion to complete
void DS18B20_DS2482::setCheckForConversion(bool flag){
    checkForConversion = flag;
}

// gets the value of the waitForConversion flag
bool DS18B20_DS2482::getCheckForConversion(){
    return checkForConversion;
}

bool DS18B20_DS2482::isConversionComplete()
{
   uint8_t b = _wire->wireReadBit();
   return (b == 1);
}

// sends command for all devices on the bus to perform a temperature conversion
void DS18B20_DS2482::requestTemperatures(){

    _wire->reset();
    _wire->wireSkip();
    //_wire->wireWriteByte(STARTCONVO, parasite);
	_wire->wireWriteByte(STARTCONVO);

    // ASYNC mode?
    if (!waitForConversion) return;
    blockTillConversionComplete(bitResolution);

}

// sends command for one device to perform a temperature by address
// returns FALSE if device is disconnected
// returns TRUE  otherwise
bool DS18B20_DS2482::requestTemperaturesByAddress(uint8_t* deviceAddress){

    uint8_t bitResolution = getResolution(deviceAddress);
    if (bitResolution == 0){
     return false; //Device disconnected
    }

    if (_wire->reset() == 0){
        return false;
    }

    _wire->wireSelect(deviceAddress);
    //_wire->wireWriteByte(STARTCONVO, parasite);
	_wire->wireWriteByte(STARTCONVO);


    // ASYNC mode?
    if (!waitForConversion) return true;

    blockTillConversionComplete(bitResolution);

    return true;

}


// Continue to check if the IC has responded with a temperature
void DS18B20_DS2482::blockTillConversionComplete(uint8_t bitResolution){
    
    int delms = millisToWaitForConversion(bitResolution);
    if (checkForConversion && !parasite){
        unsigned long now = millis();
        while(!isConversionComplete() && (millis() - delms < now));
    } else {
        delay(delms);
    }
    
}

// returns number of milliseconds to wait till conversion is complete (based on IC datasheet)
int16_t DS18B20_DS2482::millisToWaitForConversion(uint8_t bitResolution){

    switch (bitResolution){
    case 9:
        return 94;
    case 10:
        return 188;
    case 11:
        return 375;
    default:
        return 750;
    }

}


// sends command for one device to perform a temp conversion by index
bool DS18B20_DS2482::requestTemperaturesByIndex(uint8_t deviceIndex){

    DeviceAddress deviceAddress;
    getAddress(deviceAddress, deviceIndex);

    return requestTemperaturesByAddress(deviceAddress);

}

// Fetch temperature for device index
float DS18B20_DS2482::getTempCByIndex(uint8_t deviceIndex){

    DeviceAddress deviceAddress;
    if (!getAddress(deviceAddress, deviceIndex)){
        return DEVICE_DISCONNECTED_C;
    }

    return getTempC((uint8_t*)deviceAddress);

}

// Fetch temperature for device index
float DS18B20_DS2482::getTempFByIndex(uint8_t deviceIndex){

    DeviceAddress deviceAddress;

    if (!getAddress(deviceAddress, deviceIndex)){
        return DEVICE_DISCONNECTED_F;
    }

    return getTempF((uint8_t*)deviceAddress);

}

// reads scratchpad and returns fixed-point temperature, scaling factor 2^-7
int16_t DS18B20_DS2482::calculateTemperature(uint8_t* deviceAddress, uint8_t* scratchPad){

    int16_t fpTemperature =
    (((int16_t) scratchPad[TEMP_MSB]) << 11) |
    (((int16_t) scratchPad[TEMP_LSB]) << 3);

    /*
    DS1820 and DS18S20 have a 9-bit temperature register.

    Resolutions greater than 9-bit can be calculated using the data from
    the temperature, and COUNT REMAIN and COUNT PER ??C registers in the
    scratchpad.  The resolution of the calculation depends on the model.

    While the COUNT PER ??C register is hard-wired to 16 (10h) in a
    DS18S20, it changes with temperature in DS1820.

    After reading the scratchpad, the TEMP_READ value is obtained by
    truncating the 0.5??C bit (bit 0) from the temperature data. The
    extended resolution temperature can then be calculated using the
    following equation:

                                    COUNT_PER_C - COUNT_REMAIN
    TEMPERATURE = TEMP_READ - 0.25 + --------------------------
                                            COUNT_PER_C

    Hagai Shatz simplified this to integer arithmetic for a 12 bits
    value for a DS18S20, and James Cameron added legacy DS1820 support.

    See - http://myarduinotoy.blogspot.co.uk/2013/02/12bit-result-from-ds18s20.html
    */

    if (deviceAddress[0] == DS18S20MODEL){
        fpTemperature = ((fpTemperature & 0xfff0) << 3) - 16 +
            (
                ((scratchPad[COUNT_PER_C] - scratchPad[COUNT_REMAIN]) << 7) /
                  scratchPad[COUNT_PER_C]
            );
    }

    return fpTemperature;
}


// returns temperature in 1/128 degrees C or DEVICE_DISCONNECTED_RAW if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_RAW is defined in
// DS18B20_DS2482.h. It is a large negative number outside the
// operating range of the device
int16_t DS18B20_DS2482::getTemp(uint8_t* deviceAddress){

    ScratchPad scratchPad;
    if (isConnected(deviceAddress, scratchPad)) return calculateTemperature(deviceAddress, scratchPad);
    return DEVICE_DISCONNECTED_RAW;

}

// returns temperature in degrees C or DEVICE_DISCONNECTED_C if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_C is defined in
// DS18B20_DS2482.h. It is a large negative number outside the
// operating range of the device
float DS18B20_DS2482::getTempC(uint8_t* deviceAddress){
    return rawToCelsius(getTemp(deviceAddress));
}

// returns temperature in degrees F or DEVICE_DISCONNECTED_F if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_F is defined in
// DS18B20_DS2482.h. It is a large negative number outside the
// operating range of the device
float DS18B20_DS2482::getTempF(uint8_t* deviceAddress){
    return rawToFahrenheit(getTemp(deviceAddress));
}

// returns true if the bus requires parasite power
bool DS18B20_DS2482::isParasitePowerMode(void){
    return parasite;
}


// IF alarm is not used one can store a 16 bit int of userdata in the alarm
// registers. E.g. an ID of the sensor.
// See github issue #29

// note if device is not connected it will fail writing the data.
void DS18B20_DS2482::setUserData(uint8_t* deviceAddress, int16_t data)
{
    // return when stored value == new value
    if(getUserData(deviceAddress) == data) return;

    ScratchPad scratchPad;
    if (isConnected(deviceAddress, scratchPad))
    {
        scratchPad[HIGH_ALARM_TEMP] = data >> 8;
        scratchPad[LOW_ALARM_TEMP] = data & 255;
        writeScratchPad(deviceAddress, scratchPad);
    }
}

int16_t DS18B20_DS2482::getUserData(uint8_t* deviceAddress)
{
    int16_t data = 0;
    ScratchPad scratchPad;
    if (isConnected(deviceAddress, scratchPad))
    {
        data = scratchPad[HIGH_ALARM_TEMP] << 8;
        data += scratchPad[LOW_ALARM_TEMP];
    }
    return data;
}

// note If address cannot be found no error will be reported.
int16_t DS18B20_DS2482::getUserDataByIndex(uint8_t deviceIndex)
{
    DeviceAddress deviceAddress;
    getAddress(deviceAddress, deviceIndex);
    return getUserData((uint8_t*) deviceAddress);
}

void DS18B20_DS2482::setUserDataByIndex(uint8_t deviceIndex, int16_t data)
{
    DeviceAddress deviceAddress;
    getAddress(deviceAddress, deviceIndex);
    setUserData((uint8_t*) deviceAddress, data);
}


// Convert float Celsius to Fahrenheit
float DS18B20_DS2482::toFahrenheit(float celsius){
    return (celsius * 1.8) + 32;
}

// Convert float Fahrenheit to Celsius
float DS18B20_DS2482::toCelsius(float fahrenheit){
    return (fahrenheit - 32) * 0.555555556;
}

// convert from raw to Celsius
float DS18B20_DS2482::rawToCelsius(int16_t raw){

    if (raw <= DEVICE_DISCONNECTED_RAW)
    return DEVICE_DISCONNECTED_C;
    // C = RAW/128
    return (float)raw * 0.0078125;

}

// convert from raw to Fahrenheit
float DS18B20_DS2482::rawToFahrenheit(int16_t raw){

    if (raw <= DEVICE_DISCONNECTED_RAW)
    return DEVICE_DISCONNECTED_F;
    // C = RAW/128
    // F = (C*1.8)+32 = (RAW/128*1.8)+32 = (RAW*0.0140625)+32
    return ((float)raw * 0.0140625) + 32;

}


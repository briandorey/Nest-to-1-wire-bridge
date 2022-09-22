//#define DEBUG

#ifdef DEBUG
#define TRACE(x) Serial.print(x);
#define TRACEF(x, y) Serial.print(x, y);
#else
#define TRACE(x)
#define TRACEF(x, y) ;
#endif

#include <Arduino.h>
#include <Wire.h>
#include <DS2482.h>
#include <DS18B20_DS2482.h>
#include <DS2413.h>
#include <avr/sleep.h>
#include <avr/power.h>

DS2482 ds(0);                        // 1 wire interface
DS18B20_DS2482 DS18B20_devices(&ds); // temperature sensors
DS2413 DS2413_devices(&ds);          // 1 wire PIO switchs

int DevicesCount = 0;
int TemperatureCount = 0;
int SwitchCount = 0;

volatile int f_timer=0;



void Sleep(void)
{
  set_sleep_mode(SLEEP_MODE_IDLE);  
  
  // Disable unused peripherals
  power_adc_disable();
  power_spi_disable();
  power_timer0_disable();
  power_timer2_disable();
  power_twi_disable();  

  // Enter sleep mode
  sleep_enable();
  sleep_cpu ();
  
  // The program continues from here after the timer timeout*/
  sleep_disable(); /* First thing to do is disable sleep. */
  
  // Enable peripherals.
  power_all_enable();
}

void i2cDetect()
{
    for (uint8_t i2caddress = 1; i2caddress < 127; i2caddress++)
    {
        Wire.beginTransmission(i2caddress);
        uint8_t error = Wire.endTransmission();

        if (error == 0)
        {
            TRACE("I2C device found at address 0x");
            if (i2caddress < 16) TRACE("0");
            TRACEF(i2caddress, HEX);
            TRACE("\n");
        }
    }
}

void deviceCount()
{
    for (uint8_t i = 0; i < DevicesCount; i++)
    {
        DeviceAddress &address = ds.getDeviceAtIndex(i);
        if (DS18B20_devices.validFamily(address)) TemperatureCount ++;
        if (DS2413_devices.validFamily(address)) SwitchCount ++;       
    }

    TRACE("Temperature Devices: " + (String)TemperatureCount + "\n");
    TRACE("Switch Devices: " + (String)SwitchCount + "\n");
}

void getData()
{
    Serial.print("{"); // opening json

    int a = 0;

    // get temperature sensors
    Serial.print("\"temperatures\": [");
    if (TemperatureCount > 0)
    {        
        for (uint8_t i = 0; i < DevicesCount; i++)
        {
            DeviceAddress &address = ds.getDeviceAtIndex(i);
            if (DS18B20_devices.validFamily(address)){
                Serial.print("{");
                // print serial number
                String SerialNumber = "";
                for (uint8_t x = 0; x < 8; x++)
                {
                    if (address[x] < 0x10) SerialNumber += "0";
                    SerialNumber += String(address[x], HEX);
                    if (x < 7) SerialNumber += "-";
                }

                Serial.print("\"address\": \"" + SerialNumber + "\",");

                // print temperature
                Serial.print("\"value\": \"");
                DS18B20_devices.requestTemperaturesByAddress(address);
                Serial.print(DS18B20_devices.getTempC(address));
                
                if (a < (TemperatureCount - 1)) Serial.print("\"},");
                else Serial.print("\"}");
                a++;
            }
        }
    }

    Serial.print("],");
    
    // get switch sensors
    Serial.print("\"switches\": [");
    if (SwitchCount > 0)
    {
        a = 0;
        for (uint8_t i = 0; i < DevicesCount; i++)
        {
            DeviceAddress &address = ds.getDeviceAtIndex(i);
            if (DS2413_devices.validFamily(address)){
                Serial.print("{");
                // print serial number
                String SerialNumber = "";
                for (uint8_t x = 0; x < 8; x++)
                {
                    if (address[x] < 0x10) SerialNumber += "0";
                    SerialNumber += String(address[x], HEX);
                    if (x < 7) SerialNumber += "-";
                }

                Serial.print("\"address\": \"" + SerialNumber + "\",");

                // print PIO states
                int state = DS2413_devices.getPIOState(address);

                uint8_t PIOAState = 0;
                uint8_t PIOBState = 0;

                if (state & (1 << 0)) PIOAState = 1;
                if (state & (1 << 2)) PIOBState = 1;

                Serial.print("\"pioa\": \"" + String(PIOAState) + "\",");
                Serial.print("\"piob\": \"" + String(PIOBState) + "\"");
                
                if (a < (SwitchCount - 1)){
                    Serial.print("},");
                } 
                else{
                    Serial.print("}");
                }
                a++;
            }
        }
    }   
    
    Serial.print("]}\n");
}

void setup()
{

    Serial.begin(115200);

    TRACE("starting I2C: ");
    Wire.begin();
    i2cDetect();

    TRACE("DS2482-100 reset: ");
    ds.reset();

    //search for devices and print address = true
    TRACE("DS2482-100 scan: \n");
    DevicesCount = ds.devicesCount(true); // count available 1-wire devices

    deviceCount(); // get count of temperature and switch devices

    // Configure interrupt timer

    /* Normal timer operation.*/
    TCCR1A = 0x00; 
    
    /* Clear the timer counter register.
    * You can pre-load this register with a value in order to 
    * reduce the timeout period, say if you wanted to wake up
    * ever 4.0 seconds exactly.
    */
    TCNT1=0x0000; 
    
    /* Configure the prescaler for 1:1024, giving us a 
    * timeout of 4.09 seconds.
    */
    TCCR1B = 0x05;
    
    /* Enable the timer overlow interrupt. */
    TIMSK1=0x01;
}

ISR(TIMER1_OVF_vect)
{
  /* set the flag. */
   f_timer++;   
}

void loop()
{
   if(f_timer >= 4) // 20 seconds has passed
   {       
       getData();
       f_timer = 0;      
   }
   Sleep();
}

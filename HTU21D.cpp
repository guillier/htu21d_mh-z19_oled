/*
 * The MIT License (MIT)
 * Copyright (c) 2015-2017 François GUILLIER <dev @ guillier . org>
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <Wire.h>

#include "HTU21D.h"

HTU21D::HTU21D()
{
  //Set initial values for private vars
}

//Begin
/*******************************************************************************************/
//Start I2C communication
boolean HTU21D::begin(int sda, int scl)
{
    Wire.begin(sda, scl);
    return reset();
}


#define HTU21DF_RESET       0xFE

// Soft Reset
boolean HTU21D::reset()
{
    Wire.beginTransmission(HTDU21D_ADDRESS);
    Wire.write(HTU21DF_RESET);
    Wire.endTransmission();
    delay(15); // Delay after reset : 15ms (max)

    Wire.beginTransmission(HTDU21D_ADDRESS);
    Wire.write(READ_USER_REG);
    Wire.endTransmission();
    Wire.requestFrom(HTDU21D_ADDRESS, 1);
    return (Wire.read() == 2); // After reset, user register value should be 2 (i.e. Disable OTP)
}

/*******************************************************************************************/
// Read a raw value from the sensor
// Returns 998 if I2C timed out 
// Returns 999 if CRC is wrong
unsigned int HTU21D::readValue(int code)
{
    // Request a humidity reading
    Wire.beginTransmission(HTDU21D_ADDRESS);
    Wire.write(code); // Measure Value for given command
    Wire.endTransmission();

    // Delay while measurement is taken.
    delay(50); // 50ms max in 14-bit resolution

    // Comes back in three bytes, data(MSB) / data(LSB) / Checksum
    Wire.requestFrom(HTDU21D_ADDRESS, 3);

    // Wait for data to become available
    int counter = 0;
    while (Wire.available() < 3)
    {
        counter++;
        delay(1);
        if (counter > 100) return 1; // Error timeout if no complete data within 100ms
    }

    byte msb, lsb, checksum;
    msb = Wire.read();
    lsb = Wire.read();
    checksum = Wire.read();

    unsigned int rawValue = ((unsigned int) msb << 8) | (unsigned int) lsb;
    if (check_crc(rawValue, checksum) != 0) return 2; // Error out

    return rawValue & 0xFFFC; // Zero out the status bits
}


/*******************************************************************************************/
// Calc humidity and return it to the user
// Returns 998 if I2C timed out 
// Returns 999 if CRC is wrong
float HTU21D::readHumidity(void)
{
   // Request a humidity reading
    unsigned int rawHumidity = readValue(TRIGGER_HUMD_MEASURE_NOHOLD);

    if (rawHumidity < 3)
        return 997 + rawHumidity;
        
    // Given the raw humidity data, calculate the actual relative humidity
    float tempRH = rawHumidity / (float)65536;
    return ((125 * tempRH) - 6);
}

/*******************************************************************************************/
// Calc temperature and return it to the user
// Returns 998 if I2C timed out 
// Returns 999 if CRC is wrong
float HTU21D::readTemperature(void)
{
    //Request the temperature
    unsigned int rawTemperature = readValue(TRIGGER_TEMP_MEASURE_NOHOLD);

    if (rawTemperature < 3)
        return 997 + rawTemperature;

    // Given the raw temperature data, calculate the actual temperature
    float tempTemperature = rawTemperature / (float)65536;
    return ((175.72 * tempTemperature)-46.85);
}

//Give this function the 2 byte message (measurement) and the check_value byte from the HTU21D
//If it returns 0, then the transmission was good
//If it returns something other than 0, then the communication was corrupted
//From: http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
//POLYNOMIAL = 0x0131 = x^8 + x^5 + x^4 + 1 : http://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks
#define SHIFTED_DIVISOR 0x988000 //This is the 0x0131 polynomial shifted to farthest left of three bytes

byte HTU21D::check_crc(uint16_t message_from_sensor, uint8_t check_value_from_sensor)
{
    uint32_t remainder = (uint32_t)message_from_sensor << 8; // Pad with 8 bits because we have to add in the check value
    remainder |= check_value_from_sensor; // Add on the check value

    uint32_t divisor = (uint32_t)SHIFTED_DIVISOR;

    for (int i = 0 ; i < 16 ; i++) // Operate on only 16 positions of max 24. The remaining 8 are our remainder and should be zero when we're done.
    {
        if( remainder & (uint32_t)1<<(23 - i) ) // Check if there is a one in the left position
            remainder ^= divisor;

        divisor >>= 1; // Rotate the divisor max 16 times so that we have 8 bits left of a remainder
    }
    return (byte)remainder;
}

boolean HTU21D::get_serial_number (byte sn[])
{ 
    // Read from memory location 1
    Wire.beginTransmission(HTDU21D_ADDRESS);
    Wire.write(SERIAL_NUMBER_1A);
    Wire.write(SERIAL_NUMBER_1B);
    Wire.endTransmission();
    Wire.requestFrom(HTDU21D_ADDRESS, 8);

    // Reset SN
    for (int i = 0; i++; i<8)
        sn[i] = 255;
        
    // Wait for data to become available
    int counter = 0;
    while (Wire.available() < 8)
    {
        counter++;
        delay(1);
        if (counter > 10) return false; // Error timeout if no complete data within 10ms
    }
    sn[5] = Wire.read();
    Wire.read(); // CRC Ignored
    sn[4] = Wire.read();
    Wire.read(); // CRC Ignored
    sn[3] = Wire.read();
    Wire.read(); // CRC Ignored
    sn[2] = Wire.read();
    Wire.read(); // CRC Ignored

    // Read from memory location 2
    Wire.beginTransmission(HTDU21D_ADDRESS);
    Wire.write(SERIAL_NUMBER_2A);
    Wire.write(SERIAL_NUMBER_2B);
    Wire.endTransmission();
    Wire.requestFrom(HTDU21D_ADDRESS, 6);
    // Wait for data to become available
    counter = 0;
    while (Wire.available() < 6)
    {
        counter++;
        delay(1);
        if (counter > 10) return false; // Error timeout if no complete data within 10ms
    }
    sn[1] = Wire.read();
    sn[0] = Wire.read();
    Wire.read(); // CRC Ignored
    sn[7] = Wire.read();
    sn[6] = Wire.read();
    Wire.read(); // CRC Ignored
    
    return true;
}


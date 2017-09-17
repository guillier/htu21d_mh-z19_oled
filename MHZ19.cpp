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
#include "MHZ19.h"

MHZ19::MHZ19()
{
  //Set initial values for private vars
}


//Begin
/*******************************************************************************************/
//Start Soft Serial communication
boolean MHZ19::begin(int rx, int tx)
{
    co2Serial = new SoftwareSerial(rx, tx);
    co2Serial->begin(9600);
    ppmTotal = 0;
    ppmCount = 0;

    byte cmd[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};
  
    co2Serial->write(cmd, 9); //request ABC = OFF
}

/*******************************************************************************************/
// Return PPM of CO2
// Returns -1 if Error
// Returns -2 if Not initialised
// Returns -3 if Invalid values
// Returns -val if Unreliable reading
int MHZ19::readCO2(void)
{
    byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
    char response[9];
  
    co2Serial->write(cmd, 9); //request PPM CO2
    co2Serial->readBytes(response, 9);

    /*

    Serial.print("--->");
    for (int i=0; i<9; i++)
    {
        Serial.print(' ');
        Serial.print(response[i], HEX);
    }
    Serial.println("");
    */

    byte crc = 0;
    for (byte i=1; i<8; i++) crc += response[i];
    crc = 255 - crc;
    crc++;

    if (crc != response[8])
    {
        for (byte i=0; i<8; i++)
        {
            if (co2Serial->peek() == 0xFF)
                return -1;
            co2Serial->read();  // Shifting to solve misalignment
        }
    }
  
    if (response[0] != 0xFF) // Wrong starting byte from co2 sensor!
      return -1;
  
    if (response[1] != 0x86) // Wrong command from co2 sensor!
      return -1;

    /* Serial.print("Temp: ");
    Serial.println(response[4] - 40); */

    int responseHigh = (int) response[2];
    int responseLow = (int) response[3];
    int ppm = (256 * responseHigh) + responseLow;

    int uHigh = (int) response[6];
    int uLow = (int) response[7];
    int u = (256 * uHigh) + uLow;

    Serial.print("CO2: ");
    Serial.print(ppm);
    Serial.print(" ");
    Serial.println(int(response[5]));

    if (u == 15000)
        return -2;

    if ((ppm < 100) || (ppm > 6000))
        return -3;

    if (response[5] != 64)
        return -ppm;  // Not reliable. For information only
    
    return ppm;
}

/*******************************************************************************************/
// Return Average PPM of CO2
// Return -1 if No values
int MHZ19::readCO2Average(bool reset)
{
    int ppm = readCO2();
    if (ppm > 0)
    {
        ppmTotal += ppm;
        ppmCount ++;
        /* Serial.print(ppmTotal / ppmCount);
        Serial.print(" ");
        Serial.print(ppmCount); */
    }

    if (ppmCount == 0)
        return -1;
    unsigned int avg = ppmTotal / ppmCount;

    if (reset)
    {
        ppmTotal = 0;
        ppmCount = 0;
    }

    return avg;
}



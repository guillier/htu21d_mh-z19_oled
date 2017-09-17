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
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "HTU21D.h"
#include "MHZ19.h"
#include "localconfig.h" // Empty file by default

#ifndef WIFI_SSID
#define WIFI_SSID "DEMO_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "DEMO_PASSPHRASE"
#endif

#ifndef MQTT_TOPIC_BASE_TH
#define MQTT_TOPIC_BASE_TH "exp/sensor-esp8266-1/data/"
#endif

#ifndef MQTT_TOPIC_BASE_CO2
#define MQTT_TOPIC_BASE_CO2 "exp/sensor-esp8266-1/data/"
#endif

#ifndef MQTT_SERVER
#define MQTT_SERVER MQTTserver(192, 168, 99, 99);
#endif

#ifndef TEMPERATURE_OFFSET
#define TEMPERATURE_OFFSET 0
#endif

const char *ssid = WIFI_SSID;
const char *pass = WIFI_PASSWORD;
String mqtt_topic_base_th = MQTT_TOPIC_BASE_TH;
String mqtt_topic_base_co2 = MQTT_TOPIC_BASE_CO2;
IPAddress MQTT_SERVER;

Adafruit_SSD1306 display(8); // Reset PIN (not used)

//Create an instance of the objects
HTU21D myHumidity;
MHZ19 myCO2;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

String sn_dht21 = "****";
unsigned long startPeriod;


void publish (String topic, String payload)
{
    if (!mqtt_client.connected())
    {
        mqtt_client.connect("arduinoClient");
        #if defined(DEBUG)
        Serial.println("MQTT Connect");
        #endif
    }
    
    mqtt_client.publish(topic.c_str(), payload.c_str());
}

// Connect to Wifi and report IP address
void start_Wifi()
{
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(450);
        #if defined(DEBUG)
        Serial.print(".");
        #endif
    }
    #if defined(DEBUG)
    Serial.println("");
    Serial.println("WiFi connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    #endif
}


bool read_dht21_serial()
{
    byte sn[8];
    if (myHumidity.get_serial_number(sn))
    {
        sn_dht21 = "";
        for (int i = 7; i >= 0; i--)
        {
            String s = String(sn[i], HEX);
            if (s.length() < 2)
                sn_dht21 += "0";
            sn_dht21 += s;
        }
        return true;
    }
    return false;
}


void setup()
{  
    // Serial Monitor
    Serial.begin(9600);
    Serial.println("ESP8266 + MH-Z19 + HTU21D + MQTT");
    // By Default      SDA = GPIO4 = D1  -  SCL = GPIO5 = D2
    // On ESP-21     : SDA = GPIO12      -  SCL = GPIO14
    myHumidity.begin();

    // On nodemcu 1.0: RX (TX of MH-Z19) = GPIO13 = D4 - TX (RX of MH-Z19) = GPIO15 = D3
    myCO2.begin(D4, D3);

    display.begin(0, 0x3C, false);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("Starting");
    display.display();    

    // Wifi
    //WiFi.config(IPAddress(192,168,42,201), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
    WiFi.begin(ssid, pass);
    start_Wifi();

    display.println("Connected");
    display.display();    

    // Start MQTT
    String payload_init;
 
    if (read_dht21_serial())
        payload_init = "{\"sn\": \"" + sn_dht21 + "\"}";
    else
        payload_init = "";

    mqtt_client.setServer(MQTTserver, 1883);
    publish("exp/esp8266-htu21d-1/status/init", payload_init);
    startPeriod = millis();
    WiFi.forceSleepBegin();

    display.println("Initialisation OK");
    display.display();
}


String dsp_humidity = "__"; 
String dsp_temperature = "__._"; 
String dsp_co2 = "____"; 

void loop()
{
    if (millis() - startPeriod >= 300000)
    {
        startPeriod = millis();    
        WiFi.forceSleepWake();
        delay(100);
        start_Wifi();
        int co2 = myCO2.readCO2Average(true);
        if (co2 > 0)
        {
            #if defined(DEBUG)
            Serial.print("CO2: ");
            Serial.print(co2);
            Serial.println(" ppm");
            #endif
            publish(mqtt_topic_base_co2 + "co2", "{\"value\": " + String(co2) + "}");
            dsp_co2 = String(co2);
        }

        float humidity = myHumidity.readHumidity();
        float temperature = myHumidity.readTemperature() + (TEMPERATURE_OFFSET);

        #if defined(DEBUG)
        Serial.print("Temperature:");
        Serial.print(String(temperature, 1));
        Serial.print("C");
        Serial.print(" Humidity:");
        Serial.print(String(humidity, 0));
        Serial.println("%");
        #endif

        if (temperature < 997)
        {
            dsp_temperature = String(temperature, 1);
            publish(mqtt_topic_base_th + "temperature", "{\"value\": " + dsp_temperature + "}");
        }
        if (humidity < 998)
        {
            dsp_humidity = String(humidity, 0);
            publish(mqtt_topic_base_th + "humidity", "{\"value\": " + dsp_humidity + "}");
        }

        WiFi.forceSleepBegin();
        
        display.clearDisplay();

        display.setTextSize(1);
        display.setCursor(0,0);
        display.println(""); // Reserved for errors/status
        display.print("   "); display.print(dsp_temperature); display.print((char)247); display.print("C   ");
        display.print(dsp_humidity); display.println("%RH");
        display.print("CO2:");
        display.setTextSize(2);
        display.print(" "); display.print(dsp_co2); display.println("ppm");
        display.display();
    } else
        if (myCO2.readCO2Average() > 0)
            delay(50000);

    delay(10000);
}

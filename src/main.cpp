#include <Arduino.h>
#include <Vector.h>
#include <TFT_eSPI.h> 
#include <SPI.h>

#include <Button2.h>
#include "esp_adc_cal.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define ADC_EN          14
#define ADC_PIN         34
#define BUTTON_1        35
#define BUTTON_2        0

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

char buff[512];
int vref = 1100;

// GPIO where the DS18B20 is connected to
const int OneWireBus = 17;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(OneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// array to hold device addresses
const uint MAX_DALLAS_THERMOS = 20;
uint storageForDeviceAddresses[MAX_DALLAS_THERMOS];
Vector<uint> thermos(storageForDeviceAddresses);

const int SensorResolution = 10;

bool showCelsius = true; 

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms)
{   
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

void button_init()
{
    btn1.setPressedHandler([](Button2 &b) {
        showCelsius = !showCelsius;

        Serial.print("Show temp in ");
        if (showCelsius) {
            Serial.println("Celsius.");
        } else {
            Serial.println("Fahrenheit.");
        }
    });

    btn2.setPressedHandler([](Button2 &b) {
        Serial.print("clear screen");
        tft.fillScreen(TFT_BLACK);
    });
}

void button_loop()
{
    btn1.loop();
    btn2.loop();
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to print the temperature for a device
float printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  if(tempC == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: Could not read temperature data");
    return tempC;
  }
  Serial.print(" Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC));
  return tempC;
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress)
{
  Serial.print("Device Address: ");
  printAddress(deviceAddress);
  Serial.print(" ");
  printTemperature(deviceAddress);
  Serial.println();
}

uint getDeviceCount() {
    Serial.println("Get Dallas addresses by index..."); // Use getAddress method to ID sensors
    uint NumDallasActive = 0;
    Serial.println(" Connected temp sensor addresses:");
    // loop to count found sensors
    DeviceAddress tempaddr;
    while (sensors.getAddress(tempaddr, NumDallasActive)) { // true if sensor found for index
        Serial.print(" IsValidAddress:"); Serial.println ( sensors.validAddress(tempaddr) );
        if (!sensors.validAddress(tempaddr)) {
            continue;
        }
        Serial.print(" IsDallasSensor:"); Serial.println ( sensors.validFamily(tempaddr) );
        if (sensors.validFamily(tempaddr)) {
            thermos.push_back(NumDallasActive);
            NumDallasActive++;
        }
        Serial.print(" Sensor "); Serial.print(NumDallasActive);
        Serial.print(" address : ");
        printAddress(tempaddr); Serial.println();
    }
    Serial.print(" TTL Dallas Device Count:");
    Serial.println(NumDallasActive, DEC);
    // report parasite power requirements
    Serial.print(" Parasite power is: ");
    if (sensors.isParasitePowerMode()) Serial.println("ON");
    else Serial.println("OFF");

    return NumDallasActive;
}

void showVoltage() {
    // report voltage level
    uint16_t v = analogRead(ADC_PIN);
    float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
    String voltage = "Voltage: " + String(battery_voltage) + "V";
    tft.setTextSize(1);
    tft.print(voltage);
    Serial.println(voltage);
}

void showTemps()
{
    static uint64_t timeStamp = 0;
    if (millis() - timeStamp > 1000) {
        timeStamp = millis();
        tft.setCursor(0, 0);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        
        sensors.requestTemperatures();

        DeviceAddress tempAddress;
        String temp;



        for (int s = 0; s < thermos.size(); s++) {
            uint deviceIndex = thermos[s];
            sensors.getAddress(tempAddress, deviceIndex);
            Serial.print("Device Index "); Serial.print(deviceIndex, DEC);
            Serial.print(" Device Address: ");
            printAddress(tempAddress);
            Serial.print(" ");
            float val = printTemperature(tempAddress);
            Serial.println();
            if (val != DEVICE_DISCONNECTED_C) {
                sprintf(buff, "%d% 8.2f%s\n",s+1,(showCelsius?val:DallasTemperature::toFahrenheit(val)),(showCelsius?"÷C":"÷F") );
            } else {
                sprintf(buff, "%d N.A.\n",s+1);
            }
            temp += buff;
            // temp += String(s+1)+ " " + ( val != DEVICE_DISCONNECTED_C ? String(val) : "N.A.") + (showCelsius?"÷C":"÷F") + "\n";            
        }
        tft.print(temp);
        //tft.drawString(temp,  tft.width() / 2, tft.height() / 2 );
        showVoltage();
    }   
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Start");
    tft.init();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setSwapBytes(true);
    tft.setRotation(0);

    // Start up the Dallas Temperature Sensor library
    sensors.begin();
    espDelay(1000);

    // locate devices on the bus
    Serial.println("Locating devices..."); 
    getDeviceCount();
    
    Serial.print("Set device resolution to:");
    Serial.println(SensorResolution, DEC);

    DeviceAddress temp;
    for(int s = 0; s < thermos.size(); s++) {
        sensors.getAddress(temp, thermos[s]);
        sensors.setResolution(temp, SensorResolution, false);
    }

    if (TFT_BL > 0) { // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
         pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
         digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
    }

    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC1_CHANNEL_6, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
        vref = adc_chars.vref;
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    } else {
        Serial.println("Default Vref: 1100mV");
    }

    button_init();    
}

void loop()
{
    showTemps();
    button_loop();
}
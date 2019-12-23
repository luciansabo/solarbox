/*************************************************************
  Download latest Blynk library here:
    https://github.com/blynkkk/blynk-library/releases/latest

  Blynk is a platform with iOS and Android apps to control
  Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build graphic interfaces for all your
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: http://www.blynk.cc
    Sketch generator:           http://examples.blynk.cc
    Blynk community:            http://community.blynk.cc
    Social networks:            http://www.fb.com/blynkapp
                                http://twitter.com/blynk_app

  Blynk library is licensed under MIT license
  This example code is in public domain.

 *************************************************************
  This example runs directly on ESP8266 chip.

  Note: This requires ESP8266 support package:
    https://github.com/esp8266/Arduino

  Please be sure to select the right ESP8266 module
  in the Tools -> Board menu!

  Change WiFi ssid, pass, and Blynk auth token to run :)
 *************************************************************/

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "SparkFun_Si7021_Breakout_Library.h"
#include <Wire.h>
#include <EEPROM.h>
#include "Config.h"

// --------------------------------------------------------

float humidity;
float temp;
float voltage = 0;
byte chargeLevel = 0;
bool isSoilDry = true;
bool isDayTime;

enum statuses
{
  STATUS_OK,
  STATUS_NOTICE,
  STATUS_WARNING,
  STATUS_DANGER
} envStatus;

enum batteryStatuses
{
  STATUS_DISCONNECTED,
  STATUS_DISCHARGED,
  STATUS_CHARGED,
  STATUS_FLOAT,
  STATUS_BULK,
  STATUS_BOOST,
  STATUS_EQUALIZE,
  STATUS_OVERVOLTAGE
} batteryStatus;

int prevEnvStatus, prevBatteryStatus;

char statusText[100], batteryStatusText[100];

WidgetLED ledWeather(V2);
WidgetLED ledBattery(V5);
WidgetLED ledSoil(V8);

//Create Instance of SI7021 temp and humidity sensor
Weather sensor;

//--------------------------------------------------------------
#define BUILTIN_LED 5

void setup()
{
  // Debug console
  Serial.begin(9600);

  // disable WiFi to prevent interferences when reading sensors
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();

  // read sensors
  readVoltage();
  sensor.begin();
  readWeather();

  // connect to wifi
  networkSetup();

  // read soil sensor
  pinMode(SOIL_SENSOR_INPUT_PIN, INPUT);
  pinMode(SOIL_SENSOR_POWER, OUTPUT);

  // prepare data
  readPreviousParameters();
  calculateParams();

  // soil moisture
  if (isDayTime)
  {
    // soil
    readSoilMoisture();
    if (isSoilDry && envStatus < STATUS_DANGER)
    {
      envStatus = STATUS_WARNING;
      strcat(statusText, " Sol uscat."); // dry soil
    }
  }

  // wait until wifi connects or timeout
  waitForWifiConnection();

  if (WiFi.status() == WL_CONNECTED)
  {
    logToBlynk();
    sendAlerts();
  }

  goToSleep();
  
}

//--------------------------------------------------------------

void loop()
{
  // no need to run Blynk because we only run the setup function
  //Blynk.run();
}

/**
 * Read parameters saved in EEPROM
 */
void readPreviousParameters()
{
  unsigned short addr = 0;
  EEPROM.begin(2);
  // read saved status
  prevEnvStatus = EEPROM.read(addr);
  prevBatteryStatus = EEPROM.read(++addr);
}

/**
 * Connect to wifi and blynk using a fixed ip and specific channel/bssid
 * Will force wake up from sleep
 * 
 * @return bool success
 */
bool networkSetup()
{
  // Disable the WiFi persistence.  The ESP8266 will not load and save WiFi settings in the flash memory.
  WiFi.persistent(false);
  WiFi.forceSleepWake();
  WiFi.mode(WIFI_STA);
  // Bring up the WiFi connection
  WiFi.config(ip, gateway, subnet, dns);
  //WiFi.begin(ssid, pass, channel, bssid);
  WiFi.begin(ssid, pass);
}

bool waitForWifiConnection()
{
  int startMillis = millis();

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(WIFI_WAIT_DELAY);
    if ((millis() - startMillis) > WIFI_CONNECTION_TIMEOUT)
    {
      Serial.println("wifiSetup(): Wifi connection timeout.");
      break;
    }
  }

  Serial.println("");

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.printf("Could not connect to %s\n", ssid);
    return false;
  }

  //Serial.printf("Connected to %s in %d ms\n", ssid, wifiTime);

  Blynk.config(BLYNK_TOKEN); // in place of Blynk.begin(auth, ssid, pass);
  Blynk.connect(BLYNK_SERVER_TIMEOUT);

  //Serial.printf("Connected to Blynk in %d ms\n", blynkTime);

  return true;
}

//--------------------------------------------------------------
/**
 * Calculates the parameters after reading the sensors
 * 
 * - envStatus
 * - statusText
 */
void calculateParams()
{
  strcpy(statusText, " ");
  envStatus = STATUS_OK;

  if (humidity > 98)
  {                                         // solar controller operates at maximum 96% humidity
    strcpy(statusText, "EXCES UMIDITATE."); // excessive humidity
    envStatus = STATUS_DANGER;
  }
  else if (humidity > 94)
  {
    envStatus = STATUS_WARNING;
    strcpy(statusText, "Avert. Umiditate."); // humidity warning
  }
  else if (humidity > 85)
  {
    envStatus = STATUS_NOTICE;
    strcpy(statusText, "Umiditate crescuta."); // humidity warning
  }

  // battery charge 0 - 40
  // Discharge: -15 ~ 50°C
  // 40°C 103%, 25°C 100%, 0°C 86%
  if (temp < -25)
  {
    envStatus = STATUS_DANGER;
    strcat(statusText, " INGHET.");
  }
  else if (temp < 0)
  {
    if (envStatus < STATUS_DANGER)
    {
      envStatus = STATUS_WARNING;
    }
    strcat(statusText, " Risc inghet."); // low temp, freezing
  }
  else if (temp < 10)
  {
    if (envStatus < STATUS_WARNING)
    {
      envStatus = STATUS_NOTICE;
    }
    strcat(statusText, " Temp. scazuta."); // low temp notice
  }
  else if (temp > 60)
  {
    envStatus = STATUS_DANGER;
    strcat(statusText, " INCENDIU."); // possible fire
  }
  else if (temp > 50)
  {
    if (envStatus < STATUS_DANGER)
    {
      envStatus = STATUS_WARNING;
    }
    strcat(statusText, " Temp. f. ridicata."); // very high temp
  }
  else if (temp > 37)
  {
    if (envStatus < STATUS_WARNING)
    {
      envStatus = STATUS_NOTICE;
    }
    strcat(statusText, " Temp. ridicata."); // high temp
  }

  float compensatedVoltage = (temp - 25) * 0.03;

  // battery voltage status
  strcpy(batteryStatusText, "");
  if (voltage > 15.55)
  {
    batteryStatus = STATUS_OVERVOLTAGE;
    strcpy(batteryStatusText, "Supra-tensiune");
    strcat(statusText, " SUPRA-TENSIUNE BATERIE."); // over-voltage
  }
  else if (voltage > 14.52 - compensatedVoltage)
  {
    batteryStatus = STATUS_EQUALIZE;
    strcpy(batteryStatusText, "Incarcare Egalizare");
  }
  else if (voltage > 14.3 - compensatedVoltage)
  {
    batteryStatus = STATUS_BOOST;
    strcpy(batteryStatusText, "Incarcare Boost");
  }
  else if (voltage > 13.7 - compensatedVoltage)
  {
    batteryStatus = STATUS_FLOAT;
    strcpy(batteryStatusText, "Incarcare Float");
  }
  else if (voltage > 13.2 - compensatedVoltage)
  {
    batteryStatus = STATUS_BULK;
    strcpy(batteryStatusText, "Incarcare Bulk");
  }
  else if (voltage > 11.9)
  {
    byte capacity = 100;
    if (temp > 25)
    {
      capacity += (temp - 25) * 0.2;
    }
    else
    {
      capacity -= (25 - temp) * 0.55;
    }
    sprintf(batteryStatusText, "Se descarca. %d %", round(capacity));
    batteryStatus = STATUS_CHARGED;
  }
  else if (voltage > 3)
  {
    batteryStatus = STATUS_DISCHARGED;
    strcat(statusText, " BATERIE DESCARCATA."); // disconnected
    strcpy(batteryStatusText, "Sub-tensiune");
  }
  else
  {
    voltage = 0;
    batteryStatus = STATUS_DISCONNECTED;
    strcat(statusText, " BATERIE DECONECTATA."); // discharged
    strcpy(batteryStatusText, "Deconectat");
  }

  chargeLevel = getSoc(voltage, temp);
  isDayTime = voltage == 0 || voltage > 13;
}

/**
 * Temperature corrected State Of Charge
 */
byte getSoc(float voltage, float temp)
{
  float voltageMap[] = {11.5, 11.80, 11.9, 12.1, 12.20, 12.4, 12.47, 12.57, 12.67, 12.75, 12.85};
  float diff;
  int i;

  if (temp > 27)
  {
    voltage += diff / 7.0 * 0.005;
  }
  else
  {
    diff = 27 - temp;
    voltage -= (diff / 7.0 * 0.005) + (diff * 0.001);
  }

  for (i = 0; i <= 10; i++)
  {
    if (voltage <= voltageMap[i])
    {
      return i * 10;
    }
  }

  return 100;
}

/**
 *  Measure Relative Humidity from the Si7021 sensor
 */

void readWeather()
{
  humidity = sensor.getRH();
  // unconnected sensor give -6
  if (humidity < 0)
  {
    humidity = NULL;
  }

  // Temperature is measured every time RH is requested.
  // It is faster, therefore, to read it from previous RH
  // measurement with getTemp() instead with readTemp()
  temp = sensor.getTemp();

  if (temp < -45)
  {
    temp = NULL;
  }
}

void readSoilMoisture()
{
  isSoilDry = (digitalRead(SOIL_SENSOR_INPUT_PIN) == HIGH);
  // disable sensor
  digitalWrite(SOIL_SENSOR_POWER, LOW);
}

/**
 * Read voltage using ADC pin on Sparkfun 8266 Thing
 * Uses an average over 10 samples
 * 
 */
void readVoltage()
{
  // read the input on analog pin 0:
  int adcValue = 0;
  const uint8_t numberOfSamples = 10;

  // discard first reading
  analogRead(A0);

  // Get sensor samples with delay and calculate the sum.
  for (int i = 0; i < numberOfSamples; i++)
  {
    // Add sample to the sum counter.
    // Analog reading goes from 0 - 1023. ADC voltage range is 0 - 1V
    adcValue += analogRead(A0);

    // Delay for 5ms. entire operation takes 50ms this way
    delay(5);
  }

  // Get the average sensor value (ignore the fraction).
  adcValue = round(adcValue / numberOfSamples);
  // determine de actual voltage by multiplying the raw value to a known calibration factor
  voltage = adcValue * VOLTAGE_CALIBRATION_FACTOR;

  Serial.println(voltage);
}

/**
 * Send alerts using Blynk
 */
void sendAlerts()
{
  unsigned short addr = 0;
  bool hasAlert = false;

  // no notification if the state is not changed
  if (prevEnvStatus == envStatus && batteryStatus == prevBatteryStatus)
  {
    return;
  }

  // todo: send this alert only once if conditions go away but not more than 10 times a day
  if (prevEnvStatus != envStatus && envStatus == STATUS_DANGER)
  {
    hasAlert = true;
  }

  if (batteryStatus != prevBatteryStatus && (batteryStatus == STATUS_DISCHARGED || batteryStatus == STATUS_OVERVOLTAGE || batteryStatus == STATUS_DISCONNECTED))
  {
    hasAlert = true;
  }

  if (hasAlert)
  {
    Blynk.email("SolarBox Alert", statusText);
    char buff[50];
    sprintf(buff, "SolarBox Alert: %s", statusText);
    Blynk.notify(buff);
  }

  // write the changed values to the appropriate address of the EEPROM.
  // these values will remain there when the board is turned off.
  addr = 0;
  if (prevEnvStatus != envStatus)
  {
    EEPROM.write(addr, envStatus);
  }

  if (batteryStatus != prevBatteryStatus)
  {
    EEPROM.write(++addr, batteryStatus);
  }
  EEPROM.commit();
  yield();
}

/**
 * Publish measurements to the Blynk service
 * 
 * Virtual Pins:
 * 
 * - V0 - `float` temperature rounded to one decimal in range -40-125 C
 * - V1 - `int` humidity in range 0-100
 * - V2 - LED widget for environment status
 * - V3 - `char` weather status text
 * - V4 -  `float` SLA battery voltage in range 0-16V
 * - V5 - LED widget for battery status
 * - V6 - `char` battery status text
 * - V7 - `byte` battery charge level
 * - V8 - `bool` soil is dry
 */
void logToBlynk()
{
  if (!Blynk.connected())
  {
    return;
  }

  Blynk.virtualWrite(0, round(temp * 10) / 10.0); // round to one decimal
  Blynk.virtualWrite(1, round(humidity * 10) / 10.0);

  switch (envStatus)
  {
  case STATUS_NOTICE:
    ledWeather.setColor(COLOR_YELLOW);
    break;
  case STATUS_WARNING:
    ledWeather.setColor(COLOR_ORANGE);
    break;
  case STATUS_DANGER:
    ledWeather.setColor(COLOR_RED);
    break;
  default:
    ledWeather.setColor(COLOR_GREEN);
  }
  ledWeather.on();

  switch (batteryStatus)
  {
  case STATUS_DISCONNECTED:
    ledBattery.setColor(COLOR_GREY);
    break;
  case STATUS_OVERVOLTAGE:
  case STATUS_DISCHARGED:
    ledBattery.setColor(COLOR_RED);
    break;
  case STATUS_EQUALIZE:
    ledBattery.setColor(COLOR_ORANGE);
    break;
  case STATUS_BOOST:
    ledBattery.setColor(COLOR_YELLOW);
    break;
  case STATUS_FLOAT:
    ledBattery.setColor(COLOR_BLUE);
    break;
  default:
    ledBattery.setColor(COLOR_GREEN);
  }

  if (isDayTime)
  {
    if (isSoilDry)
    {
      ledSoil.setColor(COLOR_ORANGE);
    }
    else
    {
      ledSoil.setColor(COLOR_BLUE);
    }
  }
  else
  {
    ledSoil.setColor(COLOR_GREY);
  }

  ledSoil.on();
  ledBattery.on();

  Blynk.virtualWrite(3, statusText);
  Blynk.virtualWrite(4, voltage);
  Blynk.virtualWrite(6, batteryStatusText);
  Blynk.virtualWrite(7, chargeLevel);
  Blynk.run();
}

//---------------------------------------------------------------

void goToSleep()
{
  unsigned short seconds;

  if (envStatus == STATUS_DANGER || batteryStatus == STATUS_OVERVOLTAGE)
  {
    // 30 sec sleep if danger
    seconds = 30;
  }
  else if (envStatus != STATUS_DANGER && (voltage > 12.3 && voltage < 13))
  {
    // 3 mins sleep at night if nothing happens
    seconds = 180;
  }
  else
  {
    // normal 1 min sleep
    seconds = 60;
  }
  ESP.deepSleep(seconds * 1000000);
}

//---------------------------------------------------------------

void debugInfo()
{
  //This function prints the weather data out to the default Serial Port

  Serial.print("Temp:");
  Serial.print(temp);
  Serial.print("C, ");

  Serial.print("Humidity:");
  Serial.print(humidity);
  Serial.println("%");

  Serial.print("Status");
  Serial.println(statusText);

  Serial.print("Voltage:");
  Serial.println(voltage);
  Serial.printf("SoC: %d\n", chargeLevel);
}

# SolarBox Monitor

**Code for a circuit monitoring temperature, humidity, SLA battery voltage and state of charge.**  
*A typical usage is to monitor a box containing a solar charge controller and a battery which is kept outside.*

### Components used:
- Sparkfun ESP8266 Thing (https://www.sparkfun.com/products/13804)
- I2C Humidity and temperature sensor: Si7021 (https://www.sparkfun.com/products/13763)
- Analog/Digital Soil moisture sensor such as FC-28
- voltage divider for measuring battery voltage
[[https://preview.ibb.co/fXzdFo/voltage_divider.png]]

### How to use

1. Setup the circuitry
   useful reading for ESP8266 Thing dev board hardware setup: https://learn.sparkfun.com/tutorials/esp8266-thing-development-board-hookup-guide/hardware-setup
2. Setup the ESP8266 board in Arduino IDE
   https://learn.sparkfun.com/tutorials/esp8266-thing-development-board-hookup-guide/setting-up-arduino
3. Create/use a Blynk account
4. Copy Config.h.sample to Config.h then set there your wifi connection params, blynk key and other options
5. Compile and update the firmware
6. Add a jumper to allow deep sleep
   Connects GPIO16 (XPD) to the RST pin. (info can be found in ESP8266 manual)

### Connections

- Si7021 connected by I2C pins
- GPIO pin 13 connected to soil moisture sensor VCC. This powers up and shuts down the sensor before and after readings 
- voltage reduced by voltage divider goes to ADC pin (should be betwen 0-1 volts)

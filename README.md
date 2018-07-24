# Heatseek Feather Sensor

## Building Project

1. Ensure you're on (at least) version 1.6.13 of the Arduino IDE
1. Place the contents of the libraries directory in your Arduino user libraries directory.  (Usually `~/Documents/Arduino/libraries`)
1. Set up WICED Feather using [steps here](https://learn.adafruit.com/introducing-the-adafruit-wiced-feather-wifi/).
1. Set up M0 Feather using [steps here](https://learn.adafruit.com/adafruit-feather-m0-basic-proto/setup).
1. Set up M0 WiFi Feather using [steps here](https://learn.adafruit.com/adafruit-feather-m0-wifi-atwinc1500/using-the-wifi-module). Be sure to do the version check as described.
1. In `user_config.h`, uncomment the appropriate line depending on which microprocessor board you are using.  Only one board type line in this file should be uncommented.

## Software

### Outline

1. Prompt user to set RTC clock time if necessary.  (TODO: set this by web request).
1. If last reading time exceeds reading interval, take a new reading.
1. Store the reading on the SD card.
1. Store the reading time on SD card for comparison against reading interval.

### Notes

- Always prioritize logging data to SD card.  The microprocessor should always reboot and continue taking readings if there is a problem transmitting the data.
- TODO: Ensure device is not on battery power prior to writing to SD card.

## Hardware

### Base

- Heatseek Custom FeatherWing
- 2GB+ SD Card
- [DHT22 Temp/humidity sensor](https://www.adafruit.com/product/385)
- [Adalogger FeatherWing](https://www.adafruit.com/product/2922)
- [500mAh lipo battery](https://www.adafruit.com/product/1578)
- [CR1220 battery](https://www.adafruit.com/product/380)
- [MicroUSB power adapter](https://www.adafruit.com/product/1995)
- 10K ohm resistor
- Male/Female Headers (cut to size)

### WICED WiFi

- All base parts
- [WICED WiFi Feather](https://www.adafruit.com/product/3056)

### M0 WiFi

- All base parts
- [M0 WiFi Feather](https://www.adafruit.com/product/3010)

### Cellular/GSM(2G)

- All base parts
- [M0 Feather](https://www.adafruit.com/product/2772)
- [Sticker Antenna](https://www.adafruit.com/product/3237)
- [SIM800L GSM Breakout Module](http://www.ebay.com/itm/SIM800L-Quad-band-Network-Mini-GPRS-GSM-Breakout-Module-Ships-from-California-/172265821650?hash=item281bd7d5d2:g:97gAAOSwls5Y5qFG)


### Headseek Featherwing Board

https://oshpark.com/shared_projects/iJOnNry7

TODO: Upload schematics

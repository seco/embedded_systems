# esp8266_clock_thing

An exploratory sequence of code that eventually produces as a demo a time-of-day clock with NTP synchronisation. On the first power up, it creates an open wifi access point (http://192.168.4.1) so that you can enter config data for wifi SSID and password plus time zone (relative to UTC/GMT). At subsequent starts, the system connects to an NTP server and starts time keeping. Keep the one and only user switch pressed at power up to re-enter config details, or press it during normal operation to trigger an NTP resync. The ESP8266 oscillator is quite accurate so the daily early-morning NTP resync is probably not always needed. 

The sequence of source code files have a growing file name that indicates the incremental addition of features:
. file esp8266_I2C_LCD.ino
  - I2C driving an LCD "PCF8574T backpack" display;
. file esp8266_I2C_LCD_timerIRQ.ino
  - adding timer IRQ use;
. file esp8266_I2C_LCD_timerIRQ_ntp.ino
  - adding NTP access (system uses your wifi network to access a time server);
. file esp8266_I2C_LCD_timerIRQ_ntp_ap.ino
  - adding a wifi access point for configuration data input (you get to enter local wifi network details and specify your time zone);
. file esp8266_I2C_LCD_timerIRQ_ntp_ap_nv.ino
  - adding EEPROM use for configuration store (system can save and recall config details);
. file esp8266_I2C_LCD_timerIRQ_ntp_ap_nv_dotm.ino
  - now driving SPI linked MAX7219-connected LED dot matrix displays;
. file esp8266_I2C_LCD_timerIRQ_ntp_ap_nv_dotm_DHT.ino
  - adding One-Wire protocol access to DS temperature sensors _or_ serial protocol access to a DHT sensor; and
. file esp8266_I2C_LCD_timerIRQ_ntp_ap_nv_dotm_DHT_ui.ino
  - adding a one button switch user interface.

A growing set of references at the top of the source file sequence indicates where ideas (and libraries) came from i.e. (March2016):
 *  1 What works, what doesn't -- https://github.com/esp8266/Arduino/blob/master/doc/reference.md
 *  2 Using newLiquidCrystal from https://bitbucket.org/fmalpartida/new-liquidcrystal/wiki/Home and see also http://tronixstuff.com/2014/09/24/tutorial-serial-pcf8574-backpacks-hd44780-compatible-lcd-modules-arduino/
 *  3 http://www.switchdoc.com/2015/10/iot-esp8266-timer-tutorial-arduino-ide/
 *  4 Ntp access was inspired by https://github.com/Nurgak/Electricity-usage-monitor (example) but then I read the NTPClient-Arduino example.
 *  5 Initial access point code (for input of wifi parameters) taken from  WiFiAccessPoint-Arduino code example, https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/example-sketch-ap-web-server, and http://www.esp8266.com/viewtopic.php?f=29&t=2153.
 *  6 Persistent wifi config and timezone data added to eeprom (see various Arduino examples).
 *  7 Dot matrix display uses LedControl class and font scans for digits from http://tronixstuff.com/2013/10/11/tutorial-arduino-max7219-led-display-driver-ic/
 *  8 The DHT temperature sensor support comes from Lady Ada @adafruit (install the "non-unified" DHT library).
 *  9 The one-wire DS1820 temperature sensor support comes from the example code at https://milesburton.com/Dallas_Temperature_Control_Library
 * 10 Add timezone data to eeprom storage (now holds 2 strings and an int), and check if theButton is set at power up to allow user reset (enter local access point mode and input WiFi SSID/password and timezone data)
 * 11 If theButton input is set during normal operation, resynchronise with the ntp time source (hey, a user interface!)

---------------------------------------------------------------------------

PROGRAMMING ENVIRONMENT

Host:
. Arduino 1.6.7 (or Arduino nightly from 2-Feb-2016)
. Additional Boards Manager URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - This provides Library/Arduino15/packages/esp8266/tools/xtensa-lx106-elf-gcc etc.

Target:
. ESP8266 Generic Module
. CPU Frequency:80MHz ResetMethod:ck
. Flash Mode:DIO Frequency:40MHz Size:512K
. Upload Using:Serial Speed:921600
. Programmer: USBasp

LIBRARIES

The build produced the following library information. Note that
/Users/phillip/Library/Arduino15/packages/esp8266 are provided in the
ESP8266 tools and /Users/phillip/Documents/Arduino/libraries are local.

Multiple libraries were found for "OneWire.h"
 Used: /Users/phillip/Documents/Arduino/libraries/OneWire
 Not used: /Users/phillip/Library/Arduino15/packages/esp8266/hardware/esp8266/2.0.0/libraries/OneWire
Using library ESP8266WiFi at version 1.0 in folder: /Users/phillip/Library/Arduino15/packages/esp8266/hardware/esp8266/2.0.0/libraries/ESP8266WiFi 
Using library Wire at version 1.0 in folder: /Users/phillip/Library/Arduino15/packages/esp8266/hardware/esp8266/2.0.0/libraries/Wire 
Using library LiquidCrystal in folder: /Users/phillip/Documents/Arduino/libraries/LiquidCrystal (legacy)
Using library EEPROM at version 1.0 in folder: /Users/phillip/Library/Arduino15/packages/esp8266/hardware/esp8266/2.0.0/libraries/EEPROM 
Using library OneWire at version 2.3.2 in folder: /Users/phillip/Documents/Arduino/libraries/OneWire 
Using library DallasTemperature at version 3.7.6 in folder: /Users/phillip/Documents/Arduino/libraries/DallasTemperature 
Using library LedControl at version 1.0.6 in folder: /Users/phillip/Documents/Arduino/libraries/LedControl 
Using library ESP8266WebServer at version 1.0 in folder: /Users/phillip/Library/Arduino15/packages/esp8266/hardware/esp8266/2.0.0/libraries/ESP8266WebServer 

Sketch uses 258,028 bytes (59%) of program storage space. Maximum is 434,160 bytes.
Global variables use 39,326 bytes (48%) of dynamic memory, leaving 42,594 bytes for local variables. Maximum is 81,920 bytes.

---------------------------------------------------------------------------

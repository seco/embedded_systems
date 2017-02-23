# esp8266_clock_thing

An exploratory sequence of programs eventually produced a time-of-day clock demo featuring NTP synchronisation and WiFi access point configuration. These files grew from a code sample set for an embedded systems workshop that changed from Atmel AVR processors to the ESP8266 platform in 2015/2016.

* Phillip_Clock_Thing.ino
   - On the first power up, the system creates an open wifi access point (http://192.168.4.1) so that you can enter config data for WiFi SSID and password plus time zone (relative to UTC/GMT). At subsequent starts, the system connects to an NTP server and starts time keeping. Keep the one and only user switch pressed at power up to re-enter config details, or press it during normal operation to trigger an NTP resync. The ESP8266 oscillator is quite accurate so the daily early-morning NTP resync is probably not always needed. 

* Phillip_Clock_Thing2.ino
   - Slightly cleaner code, the analogue input is used for a summer time switch, and nodemcu pin out information added to the comments.

* src/
   - The current clock resulted from the sequence of source code files (in src/).

* A growing set of references at the top of the source file sequence indicates where ideas (and libraries) came from i.e. (March2016):
   - 1 What works, what doesn't -- https://github.com/esp8266/Arduino/blob/master/doc/reference.md
   - 2 Using newLiquidCrystal from https://bitbucket.org/fmalpartida/new-liquidcrystal/wiki/Home and see also http://tronixstuff.com/2014/09/24/tutorial-serial-pcf8574-backpacks-hd44780-compatible-lcd-modules-arduino/
   - 3 http://www.switchdoc.com/2015/10/iot-esp8266-timer-tutorial-arduino-ide/
   - 4 Ntp access was inspired by https://github.com/Nurgak/Electricity-usage-monitor (example) but then I read the NTPClient-Arduino example.
   - 5 Initial access point code (for input of wifi parameters) taken from  WiFiAccessPoint-Arduino code example, https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/example-sketch-ap-web-server, and http://www.esp8266.com/viewtopic.php?f=29&t=2153.
   - 6 Persistent wifi config and timezone data added to eeprom (see various Arduino examples).
   - 7 Dot matrix display uses LedControl class and font scans for digits from http://tronixstuff.com/2013/10/11/tutorial-arduino-max7219-led-display-driver-ic/
   - 8 The DHT temperature sensor support comes from Lady Ada @adafruit (install the "non-unified" DHT library).
   - 9 The one-wire DS1820 temperature sensor support comes from the example code at https://milesburton.com/Dallas_Temperature_Control_Library
   - 10 Add timezone data to eeprom storage (now holds 2 strings and an int), and check if theButton is set at power up to allow user reset (enter local access point mode and input WiFi SSID/password and timezone data)
   - 11 If theButton input is set during normal operation, resynchronise with the ntp time source (hey, a user interface!)

Selected library files are provided in libs/.

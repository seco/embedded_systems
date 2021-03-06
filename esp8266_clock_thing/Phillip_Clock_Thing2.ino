/* -*- mode: c++; -*-
 *
 * Phillip's Clock Thing is a world clock application resulting from...
 *
 * ESP8266 DEMONSTRATION of I2C and LCD, SPI and LED dot matrix array, and One-wire.
 *         Application is a time-of-day clock using timer interrupts with ntp
 *         client calls to synchronise time daily and WiFi access point configuration
 *         with previous config stored in EEPROM. A 4 digit LED output is provided for
 *         time, temp&humidity is read from DS or DHT sensors, and a user switch allows
 *         for bootup reconfiguration and normal mode NTP re-synchronisation.
 *
 *   p.musumeci@ieee.org Jan-2016/Jan-2017
 *
 * NOTE: Section 3.1.1 "General Purpose Input/Output Interface (GPIO)",
 *       p.17 of document 0A-ESP8266__Datasheet__EN_v4.4.pdf states
 *           All digital IO pins are protected from over-voltage with a
 *           snap-back circuit connected between the pad and ground. The
 *           snap back voltage is typically about 6V, and the holding
 *           voltage is 5.8V. This provides protection from over-voltages
 *           and ESD. The output devices are also protected from reversed
 *           voltages with diodes.
 *       That is, the ESP8266 gpio pins appear to be 5V tolerant so in this work, the
 *       peripheral devices such as the PCF8574T (LCD backpack) and the MAX7219 (LED
 *       dot matrix driver) are powered by 5V with I2C and SPI lines directly connected
 *       to the ESP8266 which is powered by 3.3V. In general, the input/output lines are
 *       connected to 3.3V signals.
 *
 *  
 * REFERENCES
 *
 *  1 What works, what doesn't -- https://github.com/esp8266/Arduino/blob/master/doc/reference.md
 *  2 Using newLiquidCrystal from https://bitbucket.org/fmalpartida/new-liquidcrystal/wiki/Home and see also
 *    http://tronixstuff.com/2014/09/24/tutorial-serial-pcf8574-backpacks-hd44780-compatible-lcd-modules-arduino/
 *  3 http://www.switchdoc.com/2015/10/iot-esp8266-timer-tutorial-arduino-ide/
 *  4 Ntp access was inspired by https://github.com/Nurgak/Electricity-usage-monitor (example) but then I read
 *    the NTPClient-Arduino example.
 *  5 Initial access point code (for input of wifi parameters) taken from  WiFiAccessPoint-Arduino code example,
 *    https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/example-sketch-ap-web-server, and
 *    http://www.esp8266.com/viewtopic.php?f=29&t=2153.
 *  6 Persistent wifi config and timezone data added to eeprom (see various Arduino examples).
 *  7 Dot matrix display uses LedControl class and font scans for digits from 
 *    http://tronixstuff.com/2013/10/11/tutorial-arduino-max7219-led-display-driver-ic/
 *  8 The DHT temperature sensor support comes from Lady Ada @adafruit (install the "non-unified" DHT library).
 *  9 The one-wire DS1820 temperature sensor support comes from the example code at
 *      https://milesburton.com/Dallas_Temperature_Control_Library
 * 10 Add timezone data to eeprom storage (now holds 2 strings and an int), and check if theButton is set at
 *    power up to allow user reconfiguration (input WiFi SSID/password and timezone data)
 * 11 If theButton input is set during normal operation, re-synchronise with the ntp time source (hey, a user
 *    interface!)
 *
 * NOTE: the built-in examples and docs now include much of the above info.
 */

//  THE HARDWARE USED HERE
//  
//  1a Using a NodeMCU v1.0 module which contains the USB adapter to connect to the development host
//
//                NodeMCU v1.0 top view with esp8266 top & usb bottom
//
//          Summer       o-/\/\--GND             GND--/\/\-o
//           Time/       |         _______________         |  \Resync
//      3.3V----o o-/\/\-o--------o ADC0   GPIO16 o--------o-o o----3.3V
//                                o NC     GPIO5  o---i2c_SCL--->[]
//                                o NC     GPIO4  o---i2c_SDA--->[] Adafruit
//                            SD3 o GPIO10 GPIO0  o       GND--->[] 7segnt
//                            SD2 o GPIO9  GPIO2  o       +5V--->[] backpack
//                            SD1 o MOSI     3.3V o------------->[]
//                            CND o CS        GND o
//                            SD0 o MISO   GPIO14 o----------HSPICLK
//                            CLK o SCLK   GPIO12 o----------HSPIQ
//                                o GND    GPIO13 o---RXD2---HSPID
//                                o 3.3V   GPIO15 o---TXD2---HSPICS
//                                o EN     GPIO3  o---RXD0
//                                o RST    GPIO1  o---TXD0
//         I2C      []<-----------o GND       GND o
//         4x20     []<-----------o Vin      3.3V o
//         LCD      []<---i2c SDA  ---x--===--x---     left x == USER switch
//         backpack []<---i2c SCL       usb           right x == FLASH switch
//                                   connector
//                                   for power
//                                   & programming
////
//  1b Using an esp8266 ESP-12 e|f device on an electrodragon white daughterboard with FTDI-232RL based
//     usb-uart adapter is used in 3.3V mode, and connected as described in Figure "ESP to Serial"
//     at https://github.com/esp8266/Arduino/blob/master/doc/boards.md
//       NOTE: the circuit at https://github.com/esp8266/Arduino/blob/master/doc/ESP_to_serial.png
//             shows direct connection between the 3.3V supply from the USB-uart interface and the
//             3.3V from the ESP8266 board - you should keep these supplies separate (unless your
//             USB-uart power source really is strong enough to drive everything).
//       NOTE: The ESP-12E module on a WHITE daughterboard (0.1" pinouts) has the 3.3V regulator
//             installed on the underside and (very important) the middle bridging 0-Ohm resistor
//             removed on top. See
//             http://www.electrodragon.com/product/esp8266-smd-adapter-board-wi07-12/
//       NOTE: The WHITE daughterboard has R1 and R3 of Figure "ESP to Serial" already installed,
//             so pay careful attention to adding R2 and R4 as noted below:
//
//        ____________ESP-12 series connections (top view)____________
//
//        FTDI-RTS & add R4=10K    ___________
//        pullup to 3.3V....RESET o           o GPIO1/RX....FTDI-TX   (top right)
//                            adc o           o GPIO3/TX....FTDI-RX
//        (R1 pullup on LHS) CHPD o           o gpio5/I2C_scl
//        theButton........GPIO16 o           o gpio4/I2C_sda
//                 gpio14/SPI_sck o           o GPIO0.......FTDI-DTR & add R2=10K pullup to 3.3V
//                gpio12/SPI_miso o           o gpio2.......DHT11 temp&humidity sensor or One-Wire
//                gpio13/SPI_mosi o           o gpio15/SPI_ss (R3 pulldown on RHS)
//        +ve power supply....VCC o           o GND.........FTDI GND & power supply GND
//                                 -----------
//                               [R1] [0Ohm] [R3]
//                                      ^--- remove this 0Ohm bridge if using the 3.3V reg
//  
//  2  The PCF8574T I2C io expander can be soldered directly on to the LCD module.
//
//  3  An Adafruit 7-segment LED backpack (uses HT16K33).
//
//  4  A 4 element 8x8 LED array cascaded on an SPI bus, using the max7219 driver chip.
//  
//  THE SOFTWARE USED HERE
//  
//  1  To simplify use of the display on the I2C "LCD backpack", we take files from
//     NewliquidCrystal_1.3.4.zip (https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads)
//     to _replace_ the default library LiquidCrystal (on OS-X, check for a location such as
//     /Applications/Arduino.app/Contents/Java/libraries/LiquidCrystal into which the files of
//     NewliquidCrystal are copied).
//  
//     NOTE: the esp8266 compiler chain fails on SR (shift register) and Software protocol variants
//           of NewliquidCrystal so just remove or rename files matching {*_SR*, SI2C*, *_SI2C*}.
//           In sh, enter:  for x in *_SR* SI2C* *_SI2C* ; do echo $x ; mv ${x} ${x}_disabled ; done
//  
//  2  The ESP8266 environment provides extern C functions to specify a timer interrupt function
//     to call back when a software based timeout occurs. This uses an os_timer_t data structure
//     that is specified via os_timer_setfn() which also specifies the user call back. The timer
//     interrupt interval is set by a call to os_timer_arm() which in this example sets the timer
//     mode to continuous operation.
//  
//  3  Pin allocations are documented at
//           http://www.esp8266.com/wiki/doku.php?id=esp8266_gpio_pin_allocations
//           Booting up in a (default) UART-enabled mode: GPIO1==uart-TX, GPIO3==uart-RX.

// TODO
//
// 1 add a retry limit on the NTP sync

#define ThisRelease "2017 r9"	// some sensor attributes are concatenated
				// to give a final My_Version C-string

///////////////////////////////////////////////////////////////////////////

#define DelSec(x) delay( x * 1000 )

//
// Application configuration (and includes)
//

#define ROTATE_dm_CHAR 1	// rotate dot matrix chars
				// - the 8x8 dot matrix LED arrays can be stacked in 2 ways for a 32x8 display,
				//   which can mean that font chars need to be rotated.
				// - uses <Adafruit_LEDBackpack.h> which includes <Adafruit_GFX.h>

// Select active displays
//
#define use_7SEGMENT_DISPLAY	1	// drive an Adafruit 7-segment display
//
#define n7seg_digits 4
#define n7seg_digitsM1 (n7seg_digits-1)

//#define use_DotMatrix 1       // not used as driver negatively effects RTC accuracy

// Select addon sensor handling
//
// - temperature/humidity reading and reporting with DHT11
//#define useDHTsensor 1
// - temperature reading and reporting with DS1820
#define useDS1820sensor 1

// Check conditionals to see if a temperature sensor is present
#if defined( useDHTsensor )
#define TempMethod "dht"
#elif defined( useDS1820sensor )
#define TempMethod "ds"
#else
#define no_temperature_display 1
#define TempMethod " "
#endif

///////////////////////////////////////////////////////////////////////////
//
// Some string concatenating for versions info
//

#define My_Version    ThisRelease TempMethod	// use rD in place of vD due to 7-seg display
#define My_Hello "    he\to   " My_Version "    ----"
char* ReleaseMsg = "S/W: " My_Version " :-)";

///////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <EEPROM.h>

#include <ESP8266WebServer.h>

// Note: some other includes are scattered within the code, handling optional hardware

///////////////////////////////////////////////////////////////////////////

// define IO setup
//
// LCD geometry:
#define lcd_ROWS   4
#define lcd_COLS  20
//
// The same code drives both the 4x20 and 2x20 displays so a preference for useful
// information is placed in the first 2 rows and additional optional information is
// placed in the optional extra 2 rows. Display row allocations:
// --> eeprom reading
//     row 0	msg, ssid	msg, ssid
//     row 1	password	password
//     row 2	time zone	<not visible>
//     row 3
// --> eeprom writing
//     row 0	clr msg,         writing			   Summary (use rows 1-3 as stack)
//     row 1	         hdr msg,       ssid password timezone :-)	   ssid password timezone
//     row 2								        ssid     password
//     row 3									         ssid
// --> general use
//     row 0	Time and Temperature, and sensor state machine (ssm) status byte
//     row 1	Resync time
//     row 2	IP number
//     row 3	Port number, plus some switch events
//
//
// Instantiate LCD at I2C backpack address 0x27
// - LCD operates in 4-bit data mode
// - I2C address = PCF8574T base address 0x20 + least significant 3 bits set by user
//
#define BACKLIGHT_PIN  3        // PCF8574 connections to the LCD
#define En_pin         2        //
#define Rw_pin         1        //
#define Rs_pin         0        //
#define D4_pin         4        // b4
#define D5_pin         5        // b5
#define D6_pin         6        // b6
#define D7_pin         7        // b7
//
LiquidCrystal_I2C  lcd(0x27,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);
//
// Define lcd convenience macros and functions
//
#define LCD_gotoRowCol( row, col )	lcd.setCursor( (col), (row) )
#define lcd_setCursorSyncFlag           lcd.setCursor(lcd_COLS-2,1)
//
void clear_lcd_row( int r ) {           // overwrites text with ' ' chars
  lcd.setCursor(0,r);
  for ( int i=0; i<lcd_COLS; i++ ) lcd.print(' ');
  lcd.setCursor(0,r);
}

//
// GPIO:
// - default TX/RX pins
#define uartTXpin  1
#define uartRXpin  3

// - user reset switch
// -- when using an esp8266 on the white daughterboard
#define theButton 16		// gpio16 has a 4k7 pull down resistor and a
#define theButtonActive HIGH	// switch connecting it to 3.3V, press for HIGH

// - SPI pin assignments
#define SPI_miso  12
#define SPI_mosi  13
#define SPI_sck   14    // shift clock
#define SPI_ss    15    // device select
//- I2C pin assignments
#define I2C_scl    5
#define I2C_sda    4
// - temperature sensor IO pin
#define DHTPIN     2    // DHT module uses bidirectional serial protocol.
#define DSPIN      2    // Dallas Semi 1-wire protocol handles temperature sensor.
                        // Alternative IO pins: GPIO0 and if SPI input can be
                        // disabled, GPIO12 might also be an option

///////////////////////////////////////////////////////////////////////////

// The sensor state machines can make use of an activity spinning cursor (or equiv.)
int ssm_indexp;	// access via get/clr/inc functions
int get_ssm_index() {                                    return ssm_indexp; }
int clr_ssm_index() { ssm_indexp = 0;                    return 0; }
int inc_ssm_index() { ssm_indexp = 0x7 & (1+ssm_indexp); return ssm_indexp; }
// - define 8 step symbols:           .     o    []     o     .     o    []     o
unsigned char ssm_transitions[] = {0xa5, 0xa1, 0xdb, 0xa1, 0xa5, 0xa1, 0xdb, 0xa1};
// - define 8 spinning cursor chars (use if font version is A02)
//unsigned char ssm_transitions[] = "-\\|/-\\|/";
void ssm_update_status() {
  char* status=" ";			// NOTE: var ssm_index is not set up for DHT sensor
  status[0] = (char) ssm_transitions[get_ssm_index()];
  LCD_gotoRowCol( 0, 19 );
  lcd.print(status);
  inc_ssm_index();
}
void ssm_clear_status() {
  LCD_gotoRowCol( 0, 19 );
  lcd.print(" ");	// overprint
  clr_ssm_index();
}

///////////////////////////////////////////////////////////////////////////

// define default time zone and daily synchronisation time
#define DefaultTimeZone (+10)
// (time sync each day at TimeSync_hours:TimeSync_mins:TimeSync_secs)
#define TimeSync_hours (2)
#define TimeSync_mins  (3)
#define TimeSync_secs  (4)

// Choose 1/4 second resolution for maintaining daytime
#define Timer_IRQ_Interval_ms 250
#define Timer_IRQs_per_second   4
#define ntp_TIMEOUT (8*Timer_IRQs_per_second)  // retry every 8s

#if defined( use_7SEGMENT_DISPLAY )

#include <Adafruit_LEDBackpack.h>
//#include <Adafruit_GFX.h>

// set Adafruit driver address
#define AddressOf7segDisplay 0x70

// instantiate Adafruit 7-segment driver
Adafruit_7segment the7segDisplay = Adafruit_7segment();

// define some 7-segment activity functions that create a rotating
// individual segment pattern around the outer segments of the LHS
// digit, one for negative rotation and one for positive
uint8_t active_a=0;
void activity_7seg_neg() {
  active_a = 0xff & (active_a << 1);
  if ((active_a==0)||(active_a==64)) active_a=1;
  the7segDisplay.writeDigitRaw(0,active_a);
  the7segDisplay.writeDisplay();
}
void activity_7seg_pos() {
  active_a = 0xff & (active_a >> 1);
  if (active_a==0) active_a=32;
  the7segDisplay.writeDigitRaw(0,active_a);
  the7segDisplay.writeDisplay();
}
void activity_7seg_clear() {
  active_a = 0;
  the7segDisplay.writeDigitRaw(0,active_a);
  the7segDisplay.writeDisplay();
}
// Note: To control the colon and decimal points, use writeDigitRaw(location, bitmap)
//       with location=2 and the bits mapped as:
//         0x02 - center colon
//         0x04 - left colon - lower dot
//         0x08 - left colon - upper dot
//         0x10 - decimal point

#endif

// end of config

///////////////////////////////////////////////////////////////////////////
//
// Timer interrupts
//

extern "C" {
#include "user_interface.h"
}

os_timer_t myTimer_variable;

int clock_tick;       // increment in IRQ_handler()
                      // tested/read/cleared in main()
#define FSM_OFF (-1)
int fsm_tick=FSM_OFF; // increment in IRQ_handler()
                      // used by state machines that need timing
                      // set to FSM_OFF when inactive
                  
int clock_colon;                // flashing colon control
int dt_secs, dt_mins, dt_hours; // daytime keeping vars

int sensorRead_tick;

//
// Variables for time display
#define time_slen 12            // handles dd:dd:dd0
char time_str[time_slen];       // new computed value
char lcd_data[time_slen];       // value currently on LCD

// define timerCallback (minimal length)
void timerCallback(void *pArg) {
  clock_tick++;
  sensorRead_tick++;
  if (fsm_tick >= 0)
    fsm_tick++;
}

// - calls ESP timer config and interrupt call back setup functions
void setup_timer_interrupt_system()
{
  // attach our time keeping function to ESP's timer call back by calling
  // void os_timer_setfn() with arguments
  //     os_timer_t *pTimer          ... address of time keeping data structure
  //     os_timer_func_t *pFunction  ... address of function to call with
  //                                     prototype void (*functionName)(void *pArg)
  //     void *pArg                  ... optional args
  //
  // Note: call os_timer_setfn() before os_timer_arm()

  os_timer_setfn( &myTimer_variable, timerCallback, NULL);

  // set up 1s time keeping interrupts by calling
  // void os_timer_arm() with arguments
  //     os_timer_t *pTimer          ... time keeping data structure
  //     uint32_t milliseconds       ... millisecs/interrupt
  //     bool repeat                 ... continuous if true

  os_timer_arm( &myTimer_variable, Timer_IRQ_Interval_ms, true);
}

///////////////////////////////////////////////////////////////////////////
//
// Common IO functions allow a single call to update LCD display and Serial port
//
char * msg_sep = "----------------------------------------";

void allPrint( char* x ) {
  Serial.print(x);
  lcd.print(x);
}
void allPrintln( char* x ) {
  Serial.println(x);
  lcd.print(x);
}
void allPrintS( String x ) {
  allPrint( (char *) x.c_str() );
}
void allPrintlnS( String x ) {
  allPrintln( (char *) x.c_str() );
}
void allPrintN( int x ) {
  allPrint( (char *) String(x).c_str() );
}
void allPrintlnN( int x ) {
  allPrintln( (char *) String(x).c_str() );
}
  
///////////////////////////////////////////////////////////////////////////
//
// Sequencing of slow events
//
// This application manages a number of relatively slow peripherals and
// processes. As the real-time interrupt handler timerCallback() is being
// triggered with a frequency of Timer_IRQs_per_second, the interrupt handler
// increments a variable sensorRead_tick which can then used for trigering
// user chosen actions.  When sensorRead_tick reaches the value sensorRollover
// = 10*Timer_IRQs_per_second, it is reset to 0 i.e. sensorRead_tick restarts
// counting from 0 every 10 seconds.
//
int new_second;
//
const int sensorRollover             = 10*Timer_IRQs_per_second;  // sensors are read every 10s

// Define some constant values that, on a match with sensorRead_tick, mean
// that certain actions or events are triggered at certain time instants:
//
const int instant_to_readHumidity    =  3*Timer_IRQs_per_second;  // read H at 3s point
const int instant_to_readTemperature =  6*Timer_IRQs_per_second;  // read T at 6s point
const int instant_to_readDStemp      =  5*Timer_IRQs_per_second;  // read T at 5s point
//
// When a particular time instant is reached and an action triggered, a
// corresponding "in progress" flag is set to prevent further triggering.
// There is an X_inProgress flag for each instant_to_X defined above.
//
byte ds_temp_read_inProgress, dht_humidity_read_inProgress, dht_temp_read_inProgress;
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
#if defined( useDS1820sensor )

// OneWire DS18S20, DS18B20, DS1822 Temperature Example
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire  ds(DSPIN);
DallasTemperature DS18B20(&ds);

#define nCharsTemp 10
char latest_ds_temperature[nCharsTemp]; 
char* get_temperature() { return latest_ds_temperature; }

// Notes:
//   - assumes only one sensor in operation (Index==0)
//   - applies a hard -ve limit so result is >-100
void update_ds_temperature() {
  DS18B20.requestTemperatures();
  delay(0);
  float temp = DS18B20.getTempCByIndex(0);
  delay(0);

  if (temp <= -100) {
    strncpy(latest_ds_temperature, "?", 2);
  } else {
    String x = String(temp);
    strncpy(latest_ds_temperature, x.c_str(), nCharsTemp - 1);
  }
}

void ow_setup() {
  ds_temp_read_inProgress = 0;
  for (int i = 0; i < nCharsTemp; i++)
    latest_ds_temperature[i] = 0;

  clear_lcd_row(3);             // clear screen and storage
  allPrint("Current Temp=");
  DS18B20.begin();
  update_ds_temperature();
  allPrintln(get_temperature());
}
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#if defined( useDHTsensor )

// Temperature reading code is derived from the example testing sketch for various DHT
// humidity/temperature sensors written by ladyada and placed in the public domain.

#include "DHT.h"

// select sensor type
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Connections:
//   pin 1 (on the left) of the sensor to +3.3V (esp8266 uses 3.3V)
//   pin 2 of the sensor to whatever your DHTPIN is
//   pin 3 (on the right) of the sensor to GROUND
// Adafruit recommend a 10K pullup on the data line but the current board already has a 4k7 pullup.

// Instantiate DHT sensor
// - initialise with dht.begin()
DHT dht(DHTPIN, DHTTYPE);

#define nCharsResult 10
char latest_temperature[nCharsResult];      // char[] to hold most recent results
char latest_humidity[nCharsResult];
   
void dht_setup() {
  dht_humidity_read_inProgress = dht_temp_read_inProgress = 0;

  for (int i=0; i<nCharsResult; i++)
  {
    latest_temperature[i]=latest_humidity[i]=0;
  }
  dht.begin();
}
//
// Reading the temperature or humidity takes about 250 milliseconds so the update
// function calls are scheduled in the main loop. Calling the get_Value() functions
// return the address of the latest values.
// - Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
//
//
char* get_humidity()    { return latest_humidity; }
char* get_temperature() { return latest_temperature; }
//
void update_humidity() {
  delay(0);
  float h = dht.readHumidity();
  delay(0);

  String x = String(h);
  strncpy(latest_humidity, x.c_str(), nCharsResult - 1);
}
void update_temperature() {
  delay(0);
  float t = dht.readTemperature();
  delay(0);

  String x = String(t);
  strncpy(latest_temperature, x.c_str(), nCharsResult - 1);
}

#endif

int summer_time_adjust;
void update_summer_time() {
  int sensor = analogRead(0); // reading ADC0
  Serial.print("Updating value from ADC0/summer_time input [");
  Serial.print(sensor);
  if (sensor<50) summer_time_adjust=0;
  else           summer_time_adjust=1;
  Serial.print("] ... adjustment = ");
  Serial.println(summer_time_adjust);
}

///////////////////////////////////////////////////////////////////////////

#if defined( use_DotMatrix )

//
// Dot matrix display of digit chars
//

// define digit char scans
// - these pixel dumps for chars came from a TronixLabs code fragment
//   that has horizontal pixels bit reversed with respect to our needs
//
uint8 digits[][8] = {
  {
    B01110000,  //0
    B10001000,
    B10011000,
    B10101000,
    B11001000,
    B10001000,
    B01110000,
    6,
  },{
    B01000000,  //1
    B11000000,
    B01000000,
    B01000000,
    B01000000,
    B01000000,
    B11100000,
    4,
  },{
    B01110000,  //2
    B10001000,
    B00001000,
    B00010000,
    B00100000,
    B01000000,
    B11111000,
    6,
  },{
    B11111000,  //3
    B00010000,
    B00100000,
    B00010000,
    B00001000,
    B10001000,
    B01110000,
    6,
  },{
    B00010000,  //4
    B00110000,
    B01010000,
    B10010000,
    B11111000,
    B00010000,
    B00010000,
    6,
  },{
    B11111000,  //5
    B10000000,
    B11110000,
    B00001000,
    B00001000,
    B10001000,
    B01110000,
    6,
  },{
    B00110000,  //6
    B01000000,
    B10000000,
    B11110000,
    B10001000,
    B10001000,
    B01110000,
    6,
  },{
    B11111000,  //7
    B10001000,
    B00001000,
    B00010000,
    B00100000,
    B00100000,
    B00100000,
    6,
  },{
    B01110000,  //8
    B10001000,
    B10001000,
    B01110000,
    B10001000,
    B10001000,
    B01110000,
    6,
  },{
    B01110000,  //9
    B10001000,
    B10001000,
    B01111000,
    B00001000,
    B00010000,
    B01100000,
    6,
  },{
    B00000000,  //:
    B11000000,
    B11000000,
    B00000000,
    B11000000,
    B11000000,
    B00000000,
    3,
  },{
    B00000000,  //' '
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    8,
  }
};
const int ch_colon   = 10;
const int ch_space   = 11;
const int nDigits    = 11;  // the ' ' char is not bit reversed!
const int nChars     = 12;

const int Font_PixelsWidth  = 5;// Font data is provided in 5x7 pixel wide form
const int Font_PixelsHeight = 7;

#include <LedControl.h>
const int numDevices =  4;      // number of MAX7219s used

int led_intensity;              // current LED intensity

//                 LedControl(int dataPin, int clkPin, int csPin, int numDevices);
LedControl LedCont=LedControl(SPI_mosi, SPI_sck, SPI_ss, numDevices);  // Note: miso unused

// reset all LED driver chips
void LedCont_reset() {
  led_intensity = 8; // default brightness
  for (int x = 0; x < numDevices; x++) {
    LedCont.shutdown(x, false); // enable MAX72XX (powers up in power-saving mode)
                                // set brightness and clear the display
    LedCont.setIntensity(x, led_intensity);
    LedCont.clearDisplay(x);
  }
}

const int n_vPixels_per_digit = 7;
const int n_hPixels_per_digit = 8;
const int np_per_digit=8;	// square digits/chars have np_per_digit x np_per_digit pixels
void show_digit_scans( char x, uint8 *a_digit ) {
  Serial.print("-> "); Serial.println(x);
  for (int s = 0; s < np_per_digit; s++) {
    Serial.print("   ");
    for (int t = 0; t < np_per_digit; t++)
      if ((1<<(np_per_digit-t)) & a_digit[s]) Serial.print("*");
      else                                    Serial.print(" ");
    Serial.println();
  }
  Serial.println();
}

// reset displays and normalise font data
void LedCont_setup() {
  LedCont_reset();              // reset all LED driver chips

  // Reverse the horizontal char pixel scans i.e. reflect
  // through the Y axis. We could avoid this at run-time
  // but RAM is abundant and time does not matter...

                                    // Bit reverse each horizontal scan of each char
  for (int c=0; c<nDigits; c++) {   // . for each digit that is to be bit reversed...
    uint8 *this_digit = digits[c];
    show_digit_scans(char('0'+c), this_digit);

                                    // . for each horizontal scan (or slice) of pixels...
    for (int s = 0; s < n_vPixels_per_digit; s++) {
      uint8 tmp = 0;
                                    // . test original bits R-to-L, to control right-to-
      for (int i = 0; i < n_hPixels_per_digit; i++)
        if ((1 << i) & *(this_digit + s))
          tmp = (tmp << 1) | 1;     // left shift in a 1, or
        else
          tmp = tmp << 1;           // left shift in a 0
      // perform xtra left shifts to center char, based on the
      // width of scanned char stored in digits[c][n_vPixels_per_digit]
      *(this_digit + s) =
        tmp << ((2 + n_hPixels_per_digit - digits[c][n_vPixels_per_digit]) / 2);
    }

#if defined( ROTATE_dm_CHAR )

    // clear the scanned width (located at the position of an 8th 8b scan)
    // after the char is centered and it is of no further use
    // - can now view the digit as occupying the full 8x8 pixels
    digits[c][n_vPixels_per_digit] = 0;	//-pm-

    uint8 dbuffer[np_per_digit];
    for (int s=0;s<np_per_digit;s++) {
      dbuffer[s] = this_digit[s];
      this_digit[s] = 0;
    }
    for (int s=0;s<np_per_digit;s++)
      for (int t=0;t<np_per_digit;t++) {

	// (1<<(s)) vs. (1<<(np_per_digit-1-s)            ==> vertical flip
	// this_digit[np_per_digit-1-t] vs. this_digit[t] ==> horizontal flip
	//
	// choose a rotate by -90 degrees clockwise
	if (dbuffer[s] & (1<<t)) this_digit[t] |= (1<<(np_per_digit-1-s));
      }

#endif

  }

#if defined( ROTATE_dm_CHAR )

  for (int c = 0; c < nDigits; c++) {// . for each digit that is to be bit reversed...
    uint8 *this_digit = digits[c];
    show_digit_scans(char('0'+c), this_digit);
  }

#endif

}

//
// Digital display for clock on a 4-modules of 8x8 dot matrix arrays
// - minimalist code takes in hours/minutes and splits into 4 base 10 digits
// - after leading 0 suppression, each digit has its horizontal pixel scans
//   fed from top to bottom into the array columns
// - use arg colon to enable colon pixels when displaying tmp[1] tens-of-mins

void display_dotMatrix4digits(int dLeft, int dRight, int colon) {
  uint8 *ptr;
  int tmp[4];

  tmp[0] = dRight % 10;
  tmp[1] = dRight / 10;
  tmp[2] = dLeft % 10;
  tmp[3] = dLeft / 10;
  if (tmp[3] == 0)                      // leading hours 0 set to ' '
    tmp[3] = ch_space;
  for (int y = 0; y < 4; y++) {
    ptr = digits[tmp[y]];
    for (int x = 0; x < 7; x++) {       // 7 scans/char
      uint8 slice = *ptr++;

#if defined( ROTATE_dm_CHAR )
      if (y == 1 && (x == 0))		// colon in some reflected universe...
        slice |= (colon ? 0x24 : 0);
#else
      if (y == 1 && (x == 2 || x == 4))	// colon on left of mins-10 digit [y==1]
        slice |= (colon ? 1 : 0);
#endif

      LedCont.setColumn(y, x, slice);
    }
  }
}

#endif

///////////////////////////////////////////////////////////////////////////
//
const int ts_pads = (lcd_COLS-8)/2;
//
void display_time( char* time_str ) {   // display time string
#if defined( no_temperature_display )
  for ( int x=0; x<ts_pads; x++ ) lcd.print(' ');
#else
  lcd.print(' ');
#endif

  lcd.print( time_str );

#if !defined( no_temperature_display )
  lcd.print(" T=");
  lcd.print( get_temperature() );
#endif

#if defined( useDHTsensor )
  lcd.setCursor(0,3);                   // move to start of 4th line and overwrite
  lcd.print("          H=");
  lcd.print( get_humidity() );
#endif
}

#if defined( use_7SEGMENT_DISPLAY )
void display_7segment4digits( char* time_str, int digitsChange )  {
  if (digitsChange) {                     // NEW DATA so update Adafruit internal buffer and transmit
    the7segDisplay.drawColon(clock_colon);

    if (time_str[0]==' ')
      the7segDisplay.writeDigitRaw(0, 0);
    else
      the7segDisplay.writeDigitNum(0, 0xf & time_str[0]);
    the7segDisplay.writeDigitNum(1, 0xf & time_str[1]);
    the7segDisplay.writeDigitNum(3, 0xf & time_str[3]);
    the7segDisplay.writeDigitNum(4, 0xf & time_str[4]);

    the7segDisplay.writeDisplay();

  } else {				// NO NEW DATA so only transmit new colon to display
    the7segDisplay.drawColon(clock_colon);
    the7segDisplay.writeColon();
  }
}
void display_7segmentSetFlags( unsigned char flagsBits )  {
  the7segDisplay.displaybuffer[2] = flagsBits;
  the7segDisplay.writeColon();
}
#endif

// Define a (very basic) 7-segment font for ascii chars ' '...'z'
// From https://forum.sparkfun.com/viewtopic.php?t=2143
// but we need bits 1-7 bit reversed so...
#define BR( x ) ( ((x&0x2)<<5) | ((x&0x4)<<3) | ((x&0x8)<<1) | ((x&0x10)>>1) | ((x&0x20)>>3) | ((x&0x40)>>5) | ((x&0x80)>>7))

// The following table encodes the usual 7-segments noted a,b,c,d,e,f,g as
// bits abcdefg
//
//   aa
// f    b
// f    b
//   gg
// e    c
// e    c
//   dd
//
// The Adafruit code uses the reverse bit encoding of g,f,e,d,c,b,a so we
// employ macro BR (for bit reversal) and then re-use an extended version of
// sparkfun 7-seg font data
//
// Note: a rotating set of segments matching top, right, bottom, left is obtained
//       in the char sequence "^1_|"

const unsigned char mapASCIIto7seg[] = {
  BR(0x00), // ' ' 
  BR(0x00), // '!', No seven-segment conversion for exclamation point 
  BR(0x44), // '"', Double quote 
  BR(0x00), // '#', Pound sign 
  BR(0x00), // '$', No seven-segment conversion for dollar sign 
  BR(0x00), // '%', No seven-segment conversion for percent sign 
  BR(0x00), // '&', No seven-segment conversion for ampersand 
  BR(0x40), // ''', Single quote 
  BR(0x9C), // '(', Same as '[' 
  BR(0xF0), // ')', Same as ']' 
  BR(0x00), // '*', No seven-segment conversion for asterix 
  BR(0x00), // '+', No seven-segment conversion for plus sign 
  BR(0x00), // ',', No seven-segment conversion for comma 
  BR(0x02), // '-', Minus sign 
  BR(0x00), // '.', No seven-segment conversion for period 
  BR(0x00), // '/', No seven-segment conversion for slash 
  BR(0xFC), // '0' 
  BR(0x60), // '1', and this is also right vertical segments
  BR(0xDA), // '2' 
  BR(0xF2), // '3' 
  BR(0x66), // '4' 
  BR(0xB6), // '5' 
  BR(0xBE), // '6' 
  BR(0xE0), // '7' 
  BR(0xFE), // '8' 
  BR(0xF6), // '9' 
  BR(0x00), // ':', No seven-segment conversion for colon 
  BR(0x00), // ';', No seven-segment conversion for semi-colon 
  BR(0x00), // '<', No seven-segment conversion for less-than sign 
  BR(0x12), // '=', Equal sign 
  BR(0x00), // '>', No seven-segment conversion for greater-than sign 
  BR(0xCA), //'?', Question mark 
  BR(0x00), // '@', No seven-segment conversion for commercial at-sign 
  BR(0xEE), // 'A' 
  BR(0x3E), // 'B', Actually displayed as 'b' 
  BR(0x9C), // 'C' 
  BR(0x7A), // 'D', Actually displayed as 'd' 
  BR(0x9E), // 'E' 
  BR(0x8E), // 'F' 
  BR(0xBC), // 'G', Actually displayed as 'g' 
  BR(0x6E), // 'H' 
  BR(0x60), // 'I', Same as '1' 
  BR(0x78), // 'J' 
  BR(0x00), // 'K', No seven-segment conversion 
  BR(0x1C), // 'L' 
  BR(0x00), // 'M', No seven-segment conversion 
  BR(0x2A), // 'N', Actually displayed as 'n' 
  BR(0xFC), // 'O', Same as '0' 
  BR(0xCE), // 'P' 
  BR(0x00), // 'Q', No seven-segment conversion 
  BR(0x0A), // 'R', Actually displayed as 'r' 
  BR(0xB6), // 'S', Same as '5' 
  BR(0x1E), // 'T', Actually displayed as 't' 
  BR(0x7C), // 'U' 
  BR(0x00), // 'V', No seven-segment conversion 
  BR(0x00), // 'W', No seven-segment conversion 
  BR(0x00), // 'X', No seven-segment conversion 
  BR(0x76), // 'Y' 
  BR(0x00), // 'Z', No seven-segment conversion 
  BR(0x00), // '[' 
  BR(0x00), // '\', No seven-segment conversion 
  BR(0x00), // ']' 
  BR(0x10), // '^', Carrot ==> top segment
  BR(0x08), // '_', Underscore ==> bottom segment
  BR(0x00), // '`', No seven-segment conversion for reverse quote 
  BR(0xFA), // 'a' 
  BR(0x3E), // 'b' 
  BR(0x1A), // 'c' 
  BR(0x7A), // 'd' 
  BR(0xDE), // 'e' 
  BR(0x8E), // 'f', Actually displayed as 'F' 
  BR(0xBC), // 'g' 
  BR(0x2E), // 'h' 
  BR(0x20), // 'i' 
  BR(0x78), // 'j', Actually displayed as 'J' 
  BR(0x00), // 'k', No seven-segment conversion 
  BR(0x1C), // 'l', Actually displayed as 'L' 
  BR(0x00), // 'm', No seven-segment conversion 
  BR(0x2A), // 'n' 
  BR(0x3A), // 'o' 
  BR(0xCE), // 'p', Actually displayed as 'P' 
  BR(0x00), // 'q', No seven-segment conversion 
  BR(0x0A), // 'r' 
  BR(0xB6), // 's', Actually displayed as 'S' 
  BR(0x1E), // 't' 
  BR(0x38), // 'u' 
  BR(0x00), // 'v', No seven-segment conversion 
  BR(0x00), // 'w', No seven-segment conversion 
  BR(0x00), // 'x', No seven-segment conversion 
  BR(0x76), // 'y', Actually displayed as 'Y' 
  BR(0x00), // 'z', No seven-segment conversion 
  BR(0x00), // '{' 
  BR(0x0C), // '|', Vertical bar ==> left vertical segments
  BR(0x00), // '}'
  BR(0x00)  // '~' 
};

// load_7seg_char() updates an internal buffer but does not display result
void load_7seg_char( int location, char ch ) {
  int x = ch;
  if ((x > '~')||(x < ' ')) x=0;		// map ascii to table address used here
  else x = x - ' ';
  location = 0x3 & location;			// map 0,1,2,3 to 0,1,3,4 (as used by display)
  if (location>1) location += 1;

						// handle special mappings
  if (ch == '\t') {
    the7segDisplay.writeDigitRaw(location, 0x36);	// double-elle (bits not reversed)
    return;
  }

  the7segDisplay.writeDigitRaw(location, mapASCIIto7seg[x]);  
}
// display_7seg_str4() updates internal buffers and then displays result
void display_7seg_str4( int start, char* ptr ) {
  the7segDisplay.clear();	// clear buffer
  for (int p=0; p<4-start;p++)	// load buffer
    load_7seg_char( p+start, *(ptr+p) );
  the7segDisplay.writeDisplay();// display buffer
}
// display_7seg_int3() writes a 0..99 integer into the RHS 3 char locations
void display_7seg_int3( int value ) {
  char x[4];
  if (value >  99) value=99;
  if (value < -99) value=-99;
  sprintf( x, "%3d", value);
  display_7seg_str4( 1, x);
}

///////////////////////////////////////////////////////////////////////////
//
// The system keeps config data in eeprom. When retrieved, it is checked
// and validity determined and stored in variable configuration_is_OK.
// A valid config allows clock mode immediately at power up.
//
bool configuration_is_OK;       // ==true on valid config data retrieval

// The eeprom storage used is EEPROM_nBytesUsed (512) bytes.
// It is allocated as {header ssid_Cstr pass_Cstr misc_bytes}
#define EEPROM_nBytesUsed 512
#define WIFI_parms_size 64      // assume this is large enough for ssid and pwd
#define MISC_parms_size  8      // misc. signed bytes e.g. time zone offset
#define n_ConfigPattern  8      // use pairs of complementary bytes as pattern (pairs sum to 0xff)
const byte header[] = {0xa5,0x5a,0x69,0x96,0x55,0xaa,0xff,0};

char ssid_Cstr[WIFI_parms_size];// network SSID (name)
char pass_Cstr[WIFI_parms_size];// network password
signed char misc_bytes[MISC_parms_size];
                                // misc_bytes[0] == the_time_zone
                                // misc_bytes[1] == ...

int the_time_zone;              // current time offset with respect to UTC/GMT

bool retrieve_config_OK() {
  configuration_is_OK = false;

  EEPROM.begin(EEPROM_nBytesUsed);
  delay(10);

  // check integrity (simple byte match)
  int ptr = 0;
  for (int x = 0; x < n_ConfigPattern; x = x + 2)
    if (0xff != (EEPROM.read(ptr++) + EEPROM.read(ptr++)))
      return false;             // 2 of n_ConfigPattern bytes

  // retrieve strings
  for (int i = 0; i < WIFI_parms_size; ++i)
    ssid_Cstr[i] = char(EEPROM.read(ptr++));

  for (int i = 0; i < WIFI_parms_size; ++i)
    pass_Cstr[i] = char(EEPROM.read(ptr++));

  // retrieve ints
  for (int i = 0; i < MISC_parms_size; ++i)
    misc_bytes[i] = char(EEPROM.read(ptr++));
  the_time_zone = misc_bytes[0];

  clear_lcd_row(0);
  allPrintln("Retrieving config");
  EEPROM.end();

  DelSec( 1 );
  String x = String(" (") + ssid_Cstr + ')';
  clear_lcd_row(0); allPrintln((char*) x.c_str());

  x = String(" (") + pass_Cstr + ')';
  clear_lcd_row(1); allPrintln((char*) x.c_str());

  clear_lcd_row(2); allPrint(" (UTC/GMT ");
  if (the_time_zone > 0)  allPrint("+");
  if (the_time_zone != 0) allPrintN(the_time_zone);
  allPrint(")");

  clear_lcd_row(3);

  DelSec( 5 );
  configuration_is_OK = true;
  return true;
}

void save_config(char *s, char *p, signed char ttz) {
  EEPROM.begin(EEPROM_nBytesUsed);
  delay(10);

  //
  // Clear EEPROM region
  //
  clear_lcd_row(0);
  Serial.println();
  Serial.println();
  allPrintln("Clearing memory...");
  for (int i = 0; i < EEPROM_nBytesUsed; ++i) {
    EEPROM.write(i, 0);
    delay(1);
  }

  //
  // Write "magic" patterns to indicate valid data follows
  //
  clear_lcd_row(1);
  allPrintln("Writing header...");
  int ptr = 0;
  for (int x; x < n_ConfigPattern; x++) {
    EEPROM.write(ptr++, header[x]);
    delay(1);
  }

  //
  // Clear and load buffers for string and int data to write
  //
  for (int i = 0; i < WIFI_parms_size; ++i)
    ssid_Cstr[i] = pass_Cstr[i] = 0;        // (was setting default to ' ')
  strncpy(ssid_Cstr, s, WIFI_parms_size);   // copy network info to fixed length buffers
  strncpy(pass_Cstr, p, WIFI_parms_size);
  for (int i = 0; i < MISC_parms_size; ++i)
    misc_bytes[i] = 0;
  misc_bytes[0] = ttz;                      // copy time zone ttz etc. to fixed length buffer

  //
  // Write...
  //
  clear_lcd_row(0);
  allPrint("Writing to eeprom:");
  clear_lcd_row(1);
  for (int i = 0; i < WIFI_parms_size; ++i) {
    EEPROM.write(ptr++, ssid_Cstr[i]);
    delay(1);
  }
  allPrint("ssid");
  for (int i = 0; i < WIFI_parms_size; ++i) {
    EEPROM.write(ptr++, pass_Cstr[i]);
    delay(1);
  }
  allPrint(" password");
  for (int i = 0; i < MISC_parms_size; ++i) {
    EEPROM.write(ptr++, misc_bytes[i]);
    delay(1);
  }
  allPrint(" tz");

  //
  // Commit changes
  //
  EEPROM.end();
  allPrintln(" :-)");

  DelSec( 1 );
  Serial.println();
  clear_lcd_row(0); allPrintln("Summary");       clear_lcd_row(1);

  DelSec( 1 );
  Serial.println();
  clear_lcd_row(0); allPrintln("WiFi SSID");     clear_lcd_row(1); allPrint(" "); allPrintln(ssid_Cstr);

  DelSec( 1 );
  Serial.println();
  clear_lcd_row(0); allPrintln("WiFi Password"); clear_lcd_row(1); allPrint(" "); allPrintln(pass_Cstr);
                                                 clear_lcd_row(2); allPrint(" "); allPrintln(ssid_Cstr);
  DelSec( 1 );
  Serial.println();
  clear_lcd_row(0); allPrintln("Time Zone");     clear_lcd_row(1); allPrint(" "); allPrintlnN(int(ttz));
                                                 clear_lcd_row(2); allPrint(" "); allPrintln(pass_Cstr);
                                                 clear_lcd_row(3); allPrint(" "); allPrintln(ssid_Cstr);
  DelSec( 1 );
  Serial.println();
}

///////////////////////////////////////////////////////////////////////////

//
// WiFi for accessing local network
//

// Connection for time synchronisation
unsigned int localPort = 2390;      // Local port to listen for UDP packets
IPAddress timeServerIP;             // time.nist.gov NTP server address

//const char* ntpServerName = "time.nist.gov";
char* ntpServerName = "0.pool.ntp.org";	// try different servers via
int timeSyncCounts = 0;			//   *ntpServerName='0'+(0x3 & ++timeSyncCounts);

const int NTP_PACKET_SIZE = 48;     // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; // Buffer to hold incoming and outgoing packets

WiFiUDP udp;                        // udp structure used in udp exchanges

// - display current IP and ntp port numbers
void show_netstat(int row1, int row2) {
  int row;
  if (row1>=0) {
    row=row1 & 0x3; clear_lcd_row(row);
    allPrint("IP: ");
    allPrintlnS(WiFi.localIP().toString());
  }
  if (row2>=0) {
    row=row2 & 0x3; clear_lcd_row(row);
    allPrint("UDP local port: ");
    allPrintN(udp.localPort());
  }
  Serial.println();
}

// - connect to network and open a udp port
void setup_networking_WIFIclient() {
  clear_lcd_row(0);
  allPrint("Connect: ");
  allPrint(ssid_Cstr);
  Serial.println();
  clear_lcd_row(1);

  /**
   * Slow connections have a work-around at 
   * https://github.com/esp8266/Arduino/issues/2186#issuecomment-228581052
   * July 2016 and latest IDE --> need to call setOutputPower(0) first
   * - did not fix connection issue 13/1/2017
   *   WiFi.setOutputPower(0);
   */

   /**
     * Start Wifi connection
     * if passphrase is set the most secure supported mode will be automatically selected
     * @param ssid const char*          Pointer to the SSID string.
     * @param passphrase const char *   Optional. Passphrase. Valid characters in a passphrase must be between ASCII 32-126 (decimal).
     * @param bssid uint8_t[6]          Optional. BSSID / MAC of AP
     * @param channel                   Optional. Channel of AP
     * @return
    int begin(const char* ssid, const char *passphrase = NULL, int32_t channel = 0, uint8_t bssid[6] = NULL);
     */
  if (WiFi.status() != WL_CONNECTED) {// FIX FOR USING 2.3.0 CORE (only .begin if not connected)
    WiFi.begin(ssid_Cstr, pass_Cstr); // from https://github.com/esp8266/Arduino/issues/2186#issuecomment-228581052
    Serial.println(" [Called WiFi.begin()]");
  }
  WiFi.mode(WIFI_STA);			// ensure STA mode only
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    allPrint(".");
    if (++tries >= lcd_COLS) {
      tries = 0;
      clear_lcd_row(1);
      Serial.println();
    }
  activity_7seg_neg();
  }

  activity_7seg_clear();
  clear_lcd_row(1);
  Serial.println();

  Serial.print("Allocated an ");
  allPrint("IP: ");
  allPrintlnS(WiFi.localIP().toString());
  DelSec( 1 );          // ESP-12e seems slower than the ESP-12F part (-pm-)

  clear_lcd_row(0);
  allPrintln("Starting UDP");
  udp.begin(localPort); // [can dismantle with udp.stop()]
  delay(0);             // system call to support its real-time activities
  show_netstat(0,1);
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress &anAddress) {
  clear_lcd_row(0);
  allPrintln("Sending NTP packet..");
                                // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
                                // Initialize values needed to form NTP request
                                // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
                                // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
                                // all NTP fields initialised, send
  udp.beginPacket(anAddress, 123);// NTP request on port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

///////////////////////////////////////////////////////////////////////////

//
// WiFi for temporary access point for configuration
// - this code had a starting point of example "WiFiAccessPoint" (Feb2016)
//

// define WiFi access point ssid for configuration
// - currently ignoring a password because this mode of operation only occurs
//   when a user reboots with the config button pressed (or on initial power up)
const char *config_ssid = "ClockThing-";

ESP8266WebServer server(80);

// Two standard resonses are defined
// - provide the setup form with a definition for "update"
void handleRoot() {
  server.send(200, "text/html", "<h1>\
<p>Welcome to configuration <br> of ClockThing! <p>\
<p><form action='update' method='get'>\
SSID: <input type='text' name='s'> <p>\
PASSWORD: <input type='text' name='p'> <p>\
Time zone offset: <input type='text' name='o'> <p>\
<input type='submit' value='Submit'>\
</form> </h1>\
<p> Enter a string for SSID (wifi network name) and PASSWORD\
<p> Enter a number for time zone offset (UTC/GMT=0, Berlin=1, Canberra=10.)\
");
}
// - decode the completed form
void handleUpdate() {
  String ssid = server.arg("s");
  String pwd = server.arg("p");
  String utco = server.arg("o");

  char *ssid_ptr = (char *)ssid.c_str();
  char *pwd_ptr = (char *)pwd.c_str();
  signed char temp_byte = atoi(utco.c_str());
  save_config(ssid_ptr, pwd_ptr, temp_byte);

  clear_lcd_row(0);
  allPrintln("Restart");
  server.send(200, "text/html", "<p> <h1>Thank you!</h1><p> <h1>Please cycle power now</h1>");
  DelSec( 5 );
  clear_lcd_row(0);
  allPrintln("Please cycle power");

  // From
  //   https://stackoverflow.com/questions/39688410/how-to-switch-to-normal-wifi-mode-to-access-point-mode-esp8266
  // - need to remove AP mode otherwise the access point reappears
  //   even when connecting as a client
  WiFi.softAPdisconnect();	//
  WiFi.disconnect();		// shut down AP and
  WiFi.mode(WIFI_STA);		// also select STA mode

#if defined( use_7SEGMENT_DISPLAY )
  char* power_cycle = "    off   PLEASE     ";
  int plength = strlen(power_cycle);
  for (;;) {
    for (int p=0;p<plength-n7seg_digitsM1;p++) {
      display_7seg_str4(0,power_cycle+p);
      delay(500);
    }
  }
#else
  for (;;) delay(100);		//
#endif
}

void reconfigure_system_via_local_WIFIap() {
  DelSec( 1 );

  // create a unique tmp_ssid by appending 4 hex chars from MAC to config_ssid
  //
  char tmp_ssid[strlen(config_ssid) + 4];
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  //
  sprintf(tmp_ssid, "%s%02x%02x", config_ssid,
          toupper(mac[WL_MAC_ADDR_LENGTH - 2]),
          toupper(mac[WL_MAC_ADDR_LENGTH - 1]));
  WiFi.softAP(tmp_ssid);

  lcd.setCursor(0, 0);
  allPrintln("To configure, visit");
  lcd.setCursor(0, 1);
  allPrint("http://");
  IPAddress myIP = WiFi.softAPIP();
  allPrintS(myIP.toString());
  Serial.println();
  lcd.setCursor(0, 2);
  allPrint("on wifi network");
  lcd.setCursor(0, 3);
  allPrint("  ");
  allPrint(tmp_ssid);
  display_7seg_str4(0,"Conf");

  server.on("/", handleRoot);		// specify function called on first access "/"
  server.on("/update", handleUpdate);	// specify function called on action       "/update"
  server.begin();
}

// During setup (when config is still not OK), the default
// Arduino main loop calls this http client handler
void WIFIap_loop() {
  server.handleClient();
}

///////////////////////////////////////////////////////////////////////////

//
// Clock synchronisation via udp access to ntp server
//

void set_clock_part1() {
  fsm_tick = 0;
                                // get a random server from the pool
  *ntpServerName='0'+(0x3 & ++timeSyncCounts);
  Serial.print("Using NTP server: ");
  Serial.println( ntpServerName );

  WiFi.hostByName(ntpServerName, timeServerIP);
  delay(0);                     // system call to support its real-time activities
  sendNTPpacket(timeServerIP);  // send an NTP packet to a time server
  delay(0);                     // system call to support its real-time activities

  lcd_setCursorSyncFlag;        // mark sync as in process
  allPrint("*");
}

void set_clock_part2() {
  int cb = udp.parsePacket();
  if (!cb) {
    clear_lcd_row(0);
    if (fsm_tick > ntp_TIMEOUT) {
      allPrintln("NTP timeout, retry");
      delay(2000);
      set_clock_part1();
    } else {
      allPrintln("No packet yet");
      delay(250);
    }
  } else {
    fsm_tick = FSM_OFF;

    clear_lcd_row(0);
    allPrint("Rx pkt len=");
    allPrintN(cb);
    Serial.println();

                                            // received a packet so decode
    udp.read(packetBuffer, NTP_PACKET_SIZE);// read packet into the buffer
    delay(0);                               // system call to support its real-time activities

    // Note: the timestamp, as seconds since Jan 1 1900, starts at byte 40 of
    //       the received packet and is encoded as a big-endian 32b word
    //
    unsigned long secsSince1900 = 0;
    for (int i = 0; i < 4; i++)         // merge in 4 bytes, assuming most significant first
      secsSince1900 =
          (secsSince1900 << 8) | (unsigned long)packetBuffer[40 + i];

                                        // process time data in Unix form (seconds since
                                        // 1/1/1970) therefore subtract 70 year offset
    const unsigned long seventyYears = 2208988800UL;

                                        // epoch in Unix form of UTC aka GMT
    unsigned long epoch = secsSince1900 - seventyYears;

    int tmp_ms = dt_mins * 60 + dt_secs;// save old time in range of an hour for correction measurement

    dt_hours = (epoch % 86400L) / 3600; // convert epoch to hr,min,secs and adjust for time zone
    dt_mins = (epoch % 3600) / 60;
    dt_secs = epoch % 60;
    update_summer_time();
    dt_hours = dt_hours + the_time_zone + summer_time_adjust;
    if (dt_hours >= 24)
      dt_hours = dt_hours - 24;

    clear_lcd_row(3);	// erase resync announcement
    clear_lcd_row(1);
    allPrint("Sync@");
    sprintf(time_str, "%2d:%02d:%02d", dt_hours, dt_mins, dt_secs);

    allPrintln(time_str);
#if defined( use_7SEGMENT_DISPLAY )
    display_7segment4digits(time_str,1);
#endif

    // estimate(!) correction measurement (assume error is a few seconds)
    tmp_ms = (dt_mins * 60 + dt_secs) - tmp_ms;
    if (tmp_ms > 3600)
      tmp_ms = tmp_ms - 3600;
    if (tmp_ms >= 0)
      lcd.print(" +");
    else
      lcd.print(' ');
    lcd.print(tmp_ms);

    lcd_setCursorSyncFlag;      // mark sync as complete
    lcd.print(' ');

    show_netstat( 2,-1 );       // clear extra LCD rows and fill with IP data
    clear_lcd_row(3); allPrintln( ReleaseMsg );

    // clear pending time display updates (irrelevant as new time synchronised)
    clock_tick=clock_colon=0;
  }
}

///////////////////////////////////////////////////////////////////////////

//
// - System Initialisation
//
void setup() {

  for (int i=0; i<WIFI_parms_size; i++)
    ssid_Cstr[i] = pass_Cstr[i] = 0;
  for (int i=0; i<MISC_parms_size; i++)
    misc_bytes[i] = 0;

  Serial.begin(115200);
  DelSec( 1 );
  Serial.println();
  Serial.println(msg_sep);
  Serial.println();
  Serial.println(msg_sep);
  Serial.println("*** Initialisation ***");

  the_time_zone = DefaultTimeZone;
  update_summer_time();
                                // initialise I2C
  Wire.begin( I2C_sda, I2C_scl );
  Serial.println(" I2C");
  // Can also set transfer speed e.g. Wire.setClock( 100000 )

                                // activate LCD module for chosen format and
  lcd.begin(lcd_COLS,lcd_ROWS);
  lcd.clear();
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);       // activate backlight
  Serial.println(" LCD");
  clear_lcd_row(0); allPrintln( "Phillip's Clock" );
  clear_lcd_row(1); allPrintln( "          Thing" );
  clear_lcd_row(2); allPrintln( My_Version );

  clock_tick=clock_colon=0;
  dt_secs=dt_mins=dt_hours=0;   // clear daytime keeping vars
  sprintf( time_str, "%2d:%02d:%02d", dt_hours, dt_mins, dt_secs );

  sensorRead_tick = -1; // let this counter start at the -1/Timer_IRQs_per_second point
                        // so events do not match whole seconds

			// clear some seconds oriented flags
  new_second = 0;
  clr_ssm_index();

  setup_timer_interrupt_system();
  Serial.println(" Timer interrupts enabled");

                                // initialise time display
  lcd.home();                   // set cursor to 0,0

#if defined( use_dotMatrix )
  LedCont_setup();              // initialise LED matrix
  Serial.println(" LED matrix driver");
#endif

#if defined( use_7SEGMENT_DISPLAY )
  the7segDisplay.begin(AddressOf7segDisplay);
  the7segDisplay.clear();
  the7segDisplay.writeDisplay();

  // This code displays individual segments with an ID number
  // and helps confirm segment mapping and font data
  //
  //  int test=1;
  //  for (int cnt=0;cnt<8;cnt++) {
  //    the7segDisplay.clear();	// clear buffer
  //    the7segDisplay.writeDigitRaw(0,test);
  //    the7segDisplay.writeDigitNum(4,cnt);
  //    the7segDisplay.writeDisplay();// display buffer
  //    test = test << 1;
  //    delay(1000);
  //  }
  //  display_7segment4digits(" 0:00",1); delay(200);	// scroll some digits to show display is OK
  //  display_7segment4digits(" 0:01",1); delay(200);
  //  display_7segment4digits(" 0:12",1); delay(200);
  //  display_7segment4digits(" 1:23",1); delay(200);
  //  display_7segment4digits("12:34",1); delay(200);

  char* hello = My_Hello;

  int hlength = strlen(hello);				// display hello message wih version number
  for (int p=0;p<hlength-n7seg_digitsM1;p++) {		// (this tests the digital display)
    display_7seg_str4(0,hello+p);
    if ( *(hello+p) == 'h' ) delay(2000);
    else                     delay( 500);
  }
  
  Serial.println(" LED 7-segment driver");

  for (int p=0;p<8;p++) {				// (this tests the status bits display)
    display_7segmentSetFlags( 1<<p );
    delay( 500);
  }
#endif

#if defined( useDHTsensor )
  dht_setup();                  // initialise DHT sensor
  Serial.println(" (DHT temperature)");
#endif
#if defined( useDS1820sensor )
  ow_setup();                   // initialise 1-wire and one DS temp sensor
  Serial.println(" 1-wire driver (DS1820 temperature)");
#endif
  delay(5000);

  pinMode( theButton, INPUT );  // configure user input button
  delay(250);
  //
  // Handle re-entry of WiFi config and timezone config values when either the
  // user reset input (theButton) is set or the EEPROM config data is not available
  Serial.println(msg_sep);
  if ((digitalRead(theButton) == theButtonActive) || !retrieve_config_OK()) {
    Serial.println("Starting access point for reconfiguration");
    reconfigure_system_via_local_WIFIap();
  } else {
    Serial.println();
    Serial.println();
    Serial.println("Got config, connecting to WiFi");
    clear_lcd_row(0);
    allPrint("Time zone: UTC");
    if (the_time_zone > 0)
      allPrint("+");
    if (the_time_zone != 0)
      allPrintN(the_time_zone);

#if defined( use_7SEGMENT_DISPLAY )
    display_7seg_int3( the_time_zone );
#endif

    clear_lcd_row(0);           // move to start of 2nd line
    Serial.println();
    display_time(time_str);
    setup_networking_WIFIclient();
    set_clock_part1();          // start clock resync phases
  }
}

//
// main user function
//
void loop() {
  if (!configuration_is_OK) {
    WIFIap_loop();
    return;
  }

  if (fsm_tick != FSM_OFF)
    set_clock_part2();

  if (clock_tick >= Timer_IRQs_per_second) {
      new_second = 1;

      clock_tick=clock_tick-Timer_IRQs_per_second;  // could make this uninterruptable to be really
                                                    // sure of never losing any Timer_IRQs_per_second
                                                    // events but they are slow anyway

      clock_colon = 1 & (++clock_colon);            // update colon boolean

      // handle user input (yes, the clock has a user command input button...)
      if (digitalRead(theButton) == HIGH) {
        clear_lcd_row(3);
        allPrintln("Resyncing...");
#if defined( use_dotMatrix )
        LedCont_reset();                            // ((reset LED to allow for driver error clear))
#endif
        while (digitalRead(theButton) == HIGH)      // debounce wait
          delay(0);                                 //               on button release
        set_clock_part1();                          // trigger clock resync phases
        return;                                     //               as last action of "loop"
      }

      delay(0);                                     // system call to support its real-time activities

      int MinsChange = dt_mins;
      if (++dt_secs == 60) {                        // add 1 second to time
        dt_secs = 0;
        if (++dt_mins == 60) {
          dt_mins = 0;
          if (++dt_hours == 24)
            dt_hours = 0;
        }
      }
      MinsChange = MinsChange != dt_mins;

      if (clock_colon) sprintf( time_str, "%2d:%02d:%02d", dt_hours, dt_mins, dt_secs );
      else             sprintf( time_str, "%2d %02d %02d", dt_hours, dt_mins, dt_secs );
      delay(0);                                     // system call to support its real-time activities
      lcd.setCursor(0,0);                           // move to start of 1st row and overwrite
      delay(0);                                     // system call to support its real-time activities
      display_time( time_str );

#if defined( use_DotMatrix )
      display_dotMatrix4digits( dt_hours, dt_mins, clock_colon );// update LED dot matrix
#endif

#if defined( use_7SEGMENT_DISPLAY )
      display_7segment4digits( time_str, MinsChange );          // update LED 7-segment
#endif

      if ((dt_hours == TimeSync_hours) &&
	        (dt_mins  == TimeSync_mins ) &&
          (dt_secs == TimeSync_secs  )) {
        set_clock_part1();
      }

      delay(0);                                     // system call to support its real-time activities
    }

//
// handle sensor reads at specified intervals in a cycle of length sensorRollover
//

// Optional combined temperature and humiditiy sensor
#if defined( useDHTsensor )
  if (sensorRead_tick == instant_to_readHumidity &&
      dht_humidity_read_inProgress == 0) {
    dht_humidity_read_inProgress = 1;
    update_humidity();
  }
  if (sensorRead_tick == instant_to_readTemperature &&
      dht_temp_read_inProgress == 0) {
    //    LCD_gotoRowCol( 0, 18 );
    //    lcd.print("+");                     // update sensor active indicator
    dht_temp_read_inProgress = 1;
    update_temperature();
  }
#endif

  // Optional temperature sensor
#if defined( useDS1820sensor )
  if (sensorRead_tick == instant_to_readDStemp &&
      ds_temp_read_inProgress == 0) {
    // LCD_gotoRowCol( 0, 18 );
    // lcd.print(" +");                    // update sensor active indicator
    ds_temp_read_inProgress = 1;
    update_ds_temperature();
  }
#endif

  if ((sensorRead_tick < sensorRollover) && (new_second!=0))
    ssm_update_status();		// NOTE: var ssm_index is not set up for DHT sensor

  // Event triggering variable maintenance
  if (sensorRead_tick >= sensorRollover) {
    ssm_clear_status();
    sensorRead_tick = sensorRead_tick - sensorRollover;
    dht_humidity_read_inProgress = dht_temp_read_inProgress =
        ds_temp_read_inProgress = 0;
  }

  new_second = 0;
}

///////////////////////////////////////////////////////////////////////////

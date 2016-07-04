/**
   Greenhouse-project
   - monitor temperature and humidity. If temperature rises too high (or humidity), then open the window
   - monitor also soil humidity
   - send data to thingspeak every 10 minutes
   - note the "state" is just state of display

   Help and ideas have been gathered from many sources, thanx to all!
   Examples taken from:
   http://www.instructables.com/id/Hookup-a-16-pin-HD44780-LCD-to-an-Arduino-in-6-sec/
   http://www.instructables.com/id/Temperature-with-DS18B20/
   https://learn.adafruit.com/photocells/using-a-photocell
   http://www.instructables.com/id/Connecting-AM2320-With-Arduino/
   http://allaboutee.com/2015/01/02/esp8266-arduino-led-control-from-webpage/
   and many other resources
*/

#include <LiquidCrystal595.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include <AM2320.h>
#include "greenhouse_log.h"
#include "updateThingspeak.h"

//state handling
#define STATE_INITIAL   0
#define STATE_WORKING   1
#define STATE_SETUP     2
byte state = STATE_INITIAL;

//analog pin assignments
#define TEMP_POT_PIN     A0
#define HUM_POT_PIN      A1
#define MOIST_SENS1_PIN  A2
#define AM_USES1         A4
#define AM_USES2         A5
#define LIGHTSENS_PIN    A6

//digital pin assignments
#define LCDPIN1          2
#define LCDPIN2          3
#define LCDPIN3          4
#define BTNPIN           5
#define DALLAS_ONE_WIRE  6
#define SERVOPIN         10
#define SSERIALTX        11  //to RX on ESP-05
#define SSERIALRX        12  //to TX on ESP-05
#define ESPRESETPIN      13

// The baud rate of the serial interface
#define SERIAL_BAUD  9600
#define ESP8266_BAUD 9600

//Wifi and Thingspeak
#define WIFI_SSID "YOURWIFISSID"
#define WIFI_PWD "YOURWIFIPWD"
#define THINGSPEAK_IP "184.106.153.149"
#define TS_WKEY "TSWRITEKEY"

//global variables
int8_t  maxC = 125, minC = -125;
float   tempSensorValue = 0;
uint8_t tempLimitPotValue = 0;
float   humSensorValue = 0;
uint8_t humLimitPotValue = 0;
uint8_t buttonState = LOW;
uint8_t alertLevel  = 30;
uint8_t humLevel = 80;
uint8_t wifiCheckCntr = 0;
boolean hatchOpen = false;
uint8_t updateValuesCntr = 0;
uint8_t moisture = 100;
float  tempCase = 0; //temp inside the gadget
float  tempOutside = 0; //temp outside the greenhouse
uint16_t lightSensValue = 0;
uint8_t forecedResetCntr = 0; //forcedly reset esp-05 to tackle some hanging issues
unsigned long lastConnectionTime = 0;
const unsigned long updateThingSpeakInterval = 600000; //send values to thingspeak every 10 minutes

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(DALLAS_ONE_WIRE);

// Pass the oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

//initialize the AM2320 library
AM2320 th;

// Initialize the LCD library with the interface pins
LiquidCrystal595 lcd(LCDPIN1, LCDPIN2, LCDPIN3);
Servo servo1; // Create a servo object


//Initialize update thingspeak
UpdateThingspeak* updateTs;


/*---------------------setup and main loop---------------------*/

void setup() {
  //SERIAL_BEGIN(SERIAL_BAUD);
  Serial.begin(9600);
  delay(500);
  servo1.attach(SERVOPIN);
  servo1.write(180);  // Turn servo1 at "home" position
  delay(2000);
  servo1.detach();
  
  LOG_DEBUGLN("\nStarting up...");
  updateTs = new UpdateThingspeak(SSERIALRX, SSERIALTX, ESP8266_BAUD, ESPRESETPIN, WIFI_SSID, WIFI_PWD, THINGSPEAK_IP, TS_WKEY);
  analogReference(DEFAULT);
  pinMode(BTNPIN, INPUT);

  lcd.begin(16, 2); // Set the display to 16 columns and 2 rows
  lcd.clear();
  lcd.setCursor(0, 0);

  lcd.print("Setting up Wifi.");
  updateTs->setupWifi();

  lcd.setCursor(0, 0);
  lcd.print("Wifi is set up.");
  delay(5000);
  lcd.clear();

  pinMode(MOIST_SENS1_PIN, INPUT);
  // Initialize the library for temperature sensors
  sensors.begin();
  delay(100);

  //read values, especially potentiometer values, incase of restart
  readValues();
  alertLevel = tempLimitPotValue;
  humLevel = humLimitPotValue;

  state = STATE_INITIAL;

  // Timer0 is already used for millis() - we'll just interrupt somewhere
  // in the middle and call the "Compare A" function below
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

SIGNAL(TIMER0_COMPA_vect)
{
  /**@TODO add functionality and cleanup main loop */
  unsigned long currentMillis = millis();
}

//The main loop
void loop()
{
  lcd.setCursor(0, 0); // Set cursor to home position
  readValues();
  handleState();
  if (!(++wifiCheckCntr)) {
    if (SUCCESS != updateTs->checkIsEspResponsive()) {
      updateTs->setupWifi();
    }
  }
 
  long temp = millis();
  if ((temp - lastConnectionTime) > updateThingSpeakInterval) {
    updateTs->updateValues(tempSensorValue, humSensorValue, (hatchOpen) ? 1 : 0, moisture, tempOutside, tempCase, lightSensValue);
    lastConnectionTime = millis();
    forecedResetCntr++;
  }

  if (forecedResetCntr > 0 && !(forecedResetCntr % 72)) {
    // restart wifi as a workaround, might not help though...
    forecedResetCntr = 0;
    updateTs->setupWifi();
  }

  delay(1000);//this might not be necessary, and disturbs handling of "set" button
}


void handleState()
{
  if (buttonState == HIGH) {
    LOG_DEBUG("button pressed, state is");
    LOG_DEBUGLN(state);
    if (state == STATE_INITIAL || state == STATE_WORKING) {
      state = STATE_SETUP;
      lcd.setLED2Pin(LOW);
      lcd.setLED1Pin(LOW);
      lcd.print("");
    }
    else {
      lcd.setLED2Pin(HIGH);
      lcd.setLED1Pin(HIGH);
      lcd.print("");
      state = STATE_WORKING;
      alertLevel = tempLimitPotValue;
      humLevel = humLimitPotValue;
    }
  }

  if (state == STATE_SETUP) {
    stateSetup();
  }
  if (state == STATE_WORKING || state == STATE_INITIAL) {
    stateWorking();
  }
}

bool read_from_am2320() {
  switch (th.Read()) {
    case 2:
      LOG_DEBUGLN("CRC failed");
      return false;
      break;
    case 1:
      LOG_DEBUGLN("Sensor offline");
      return false;
      break;
    default:
      break;
  }
  return true;

}

void readSoilMoisture() {
  uint16_t tempAnalogValue = 0;
  tempAnalogValue = analogRead(MOIST_SENS1_PIN);
  delay(10);
  tempAnalogValue = analogRead(MOIST_SENS1_PIN);
  LOG_DEBUGLN(tempAnalogValue);
  moisture = uint8_t(100 * (1 - ((double)(tempAnalogValue) / 1023)));
}

void readTemperatureLimit() {
  uint16_t tempAnalogReadVal = 0;
  //read twice because of reading issues (sometimes)
  tempAnalogReadVal = analogRead(TEMP_POT_PIN); // Read the pot value
  delay(10);
  tempAnalogReadVal = analogRead(TEMP_POT_PIN); // Read the pot value
  tempLimitPotValue = map(tempAnalogReadVal, 0, 1023, 10, 100); // Map the values from 10 to 100 degrees
}

void readHumidityLimit() {
  uint16_t tempAnalogReadVal = 0;
  //read twice because of reading issues (sometimes)
  tempAnalogReadVal = analogRead(HUM_POT_PIN); // Read the pot value
  delay(10);
  tempAnalogReadVal = analogRead(HUM_POT_PIN); // Read the pot value
  humLimitPotValue = map(tempAnalogReadVal, 0, 1023, 20, 100); // Map the values from 20 to 100%
}

void readValues()
{
  //get dallas values
  sensors.requestTemperatures();

  if (true == read_from_am2320()) {
    if (isnan(th.h) || isnan(th.t)) {
      LOG_DEBUGLN("Invalid values from AM2320");
    } else {
      humSensorValue = th.h;
      tempSensorValue = th.t;
    }
  } else {
    LOG_DEBUGLN("Failed to get values from AM2320");
  }

  LOG_DEBUG("HUM: ");
  LOG_DEBUGLN(humSensorValue);
  LOG_DEBUG("TEMP: ");
  LOG_DEBUGLN(tempSensorValue);

  readSoilMoisture();
  LOG_DEBUG("SOIL: ");
  LOG_DEBUGLN(moisture);
  delay(10);
  readTemperatureLimit();
  LOG_DEBUG2("tp:");
  LOG_DEBUGLN2(tempLimitPotValue);
  delay(10);
  readHumidityLimit();
  LOG_DEBUG2("hp:");
  LOG_DEBUGLN2(humLimitPotValue);

  float tempCaseTemp = sensors.getTempCByIndex(0);
  float tempOutsTemp = sensors.getTempCByIndex(1);
  if (isnan(tempCaseTemp) || isnan(tempOutsTemp)) {
    LOG_DEBUGLN("Could not get dallas temperature");
  } else {
    tempCase = tempCaseTemp;
    tempOutside = tempOutsTemp;
  }
  LOG_DEBUG("to:");
  LOG_DEBUGLN(tempOutsTemp);
  LOG_DEBUG("tc:");
  LOG_DEBUGLN(tempCaseTemp);

  lightSensValue = analogRead(LIGHTSENS_PIN);
  LOG_DEBUG("LDR:");
  LOG_DEBUGLN(lightSensValue);

  buttonState = digitalRead(BTNPIN); // Check for button press
}

void turnServo(uint16_t degree)
{
  servo1.attach(SERVOPIN); // Attaches the servo on pin 14 to the servo1 object
  servo1.write(degree);  // Put servo1 at home position
  delay(3000);
  servo1.detach();  //detach not to disturb SoftwareSerial
}

void stateWorking()
{

  writeTempAndHumToLcd();

  if (!hatchOpen && (tempSensorValue >= alertLevel || ( humSensorValue >= humLevel && tempSensorValue >= 20))) {
    LOG_DEBUG2("Alert! over ");
    LOG_DEBUG2(alertLevel);
    LOG_DEBUG2("degrees, open hatch\n");
    turnServo(90);
    hatchOpen = true;
  }
  if (hatchOpen && ((tempSensorValue <= (alertLevel - 2) && humSensorValue <= (humLevel - 2)) || tempSensorValue <= 20)) {
    LOG_DEBUG2("Alert! less than ");
    LOG_DEBUG2(alertLevel);
    LOG_DEBUG2("(");
    LOG_DEBUG2(tempSensorValue);
    LOG_DEBUG2(") degrees, close hatch\n");
    turnServo(180);
    hatchOpen = false;
  }
}

void writeTempAndHumToLcd() {
  char float_str[10] = {0};
  lcd.setCursor(0, 0);
  dtostrf(tempSensorValue, 4, 1, float_str);
  lcd.print(float_str);
  lcd.write(B11011111); // Degree symbol
  lcd.print("C ");
  memset(&float_str, 0, sizeof(float_str));
  dtostrf(humSensorValue, 3, 1, float_str);
  lcd.print("H=");
  lcd.print(float_str);
  lcd.print("\%");

  if ((int)tempSensorValue > maxC ||
      ((int)tempSensorValue < maxC && maxC == 125 && (int)tempSensorValue != 0 )) {
    maxC = (int)tempSensorValue;
  }
  if ((int)tempSensorValue < minC && minC != -125 ||
      ((int)tempSensorValue > minC && minC == -125 && (int)tempSensorValue != 0 )) {
    minC = (int)tempSensorValue;
  }
  lcd.setCursor(0, 1);
  lcd.print("Hi=");
  lcd.print(maxC);
  lcd.write(B11011111);
  lcd.print("C Lo=");
  lcd.print(minC);
  lcd.write(B11011111);
  lcd.print("C ");
}

void displaySetup() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(tempLimitPotValue);
  lcd.print("(");
  lcd.print(alertLevel);
  lcd.print(")          ");
  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print(humLimitPotValue);
  lcd.print("(");
  lcd.print(humLevel);
  lcd.print(")        ");

}

void stateSetup()
{
  displaySetup();
}


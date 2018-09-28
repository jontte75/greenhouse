/**
   Greenhouse-project
   - monitor temperature and humidity. If temperature rises too high (or humidity), then open the window
   - monitor also soil humidity
   - note the "state" is just state of display

   Help and ideas have been gathered from many sources, thanx to all!
   E.g from:
   http://www.instructables.com/id/Hookup-a-16-pin-HD44780-LCD-to-an-Arduino-in-6-sec/
   http://www.instructables.com/id/Temperature-with-DS18B20/
   https://learn.adafruit.com/photocells/using-a-photocell
   http://www.instructables.com/id/Connecting-AM2320-With-Arduino/
   http://allaboutee.com/2015/01/02/esp8266-arduino-led-control-from-webpage/
   https://github.com/lazyscheduler/AM2320
   and many other resources
*/

#include <LiquidCrystal595.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include <AM2320.h>
#include "./log.h"

//state handling
#define STATE_INITIAL   0
#define STATE_WORKING   1
#define STATE_SETUP     2
byte state = STATE_INITIAL;

//analog pin assignments
#define TEMP_POT_PIN     A0
#define HUM_POT_PIN      A1
#define MOIST_SENS1_PIN  A2
#define LIGHTSENS_PIN    A5

//digital pin assignments
#define LCDPIN1          2
#define LCDPIN2          3
#define LCDPIN3          4
#define BTNPIN           5
#define DALLAS_ONE_WIRE  6
#define SERVOPIN         7
#define LOG_RX           10
#define LOG_TX           11 
#define AM_SCL           14
#define AM_SDA           15

// The baud rate of the serial interface
#define SERIAL_BAUD  9600
#define LOGGER_BAUD  9600

// How often should we read sensors
#define READINTERVAL (10*1000)

//global variables sent outside
volatile float   tempSensorValue;
volatile float   humSensorValue;
boolean hatchOpen = false;
volatile uint8_t moisture = 100;
volatile float  tempCase = 0; //temp inside the gadget
volatile float  tempOutside = 0; //temp outside the greenhouse
volatile uint16_t lightSensValue = 0;

//global internally used
int8_t  maxC = 125, minC = -125;
uint8_t tempLimitPotValue = 0; //these values vary, use when set
uint8_t humLimitPotValue = 0;  //these values vary, use when set
uint8_t buttonState = LOW;
uint8_t alertLevel  = 30; //alert level for temp stored from tempLimitPotValue
uint8_t humLevel    = 80; //alert level for humidy stored from humLimitPotValue
uint8_t updateValuesCntr = 0;
volatile unsigned long lastTimeReadValues = 0;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(DALLAS_ONE_WIRE);

// Pass the oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

//initialize the AM2320 library
//Note! Need to use library that supports SoftWire
//otherwise collides with other I2C 
//This library also supports multiple AM2320 sensors..
//https://github.com/lazyscheduler/AM2320 
AM2320 th(AM_SCL, AM_SDA);

// Initialize the LCD library with the interface pins
LiquidCrystal595 lcd(LCDPIN1, LCDPIN2, LCDPIN3);
Servo servo1; // Create a servo object

// Some global variables
volatile byte command = 0;

Log logger(Serial, LOGGER_BAUD, LOG_DEBUG);

/*---------------------setup and main loop---------------------*/

void setup() {
  tempSensorValue = humSensorValue = 0;
  Serial.begin(SERIAL_BAUD);

  delay(500);
  DEBUG(logger, "Starting up...\n");
  lcd.begin(16, 2); // Set the display to 16 columns and 2 rows
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set up servo.");
  turnServo(180);
  
  analogReference(DEFAULT);
  pinMode(BTNPIN, INPUT);

  pinMode(MOIST_SENS1_PIN, INPUT);

  // Initialize the library for temperature sensors
  delay(300);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set up sensors.");
  sensors.begin();
  delay(100);

  //read values, especially potentiometer values, incase of restart
  readValues(0,true);
  readConfigSensors();
  alertLevel = tempLimitPotValue;
  humLevel = humLimitPotValue;

  state = STATE_INITIAL;

  // Timer0 is already used for millis() - we'll just interrupt somewhere
  // in the middle and call the "Compare A" function below
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
  delay(300);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setups done.");
  delay(3000);
}

SIGNAL(TIMER0_COMPA_vect)
{
  unsigned long currentMillis = millis();
  readValues(currentMillis, false);
}

//The main loop
void loop()
{
  buttonState = digitalRead(BTNPIN); // Check for button press
  lcd.setCursor(0, 0); // Set cursor to home position
  handleState();
  delay(100);
}

void readConfigSensors()
{
  delay(10);
  readTemperatureLimit();
  DEBUG(logger,"tempLimit: " << tempLimitPotValue << "\n");
  delay(10);
  readHumidityLimit();
  DEBUG(logger,"humLimit: "<< humLimitPotValue << "\n");
}

void handleState()
{
  if (buttonState == HIGH) {
    DEBUG(logger,"button pressed, state is" << state << "\n");
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
  }else{
    stateWorking();
  }
}

bool read_from_am2320() {
  bool retVal = true;
  switch (th.Read()) {
    case 2:
      ERROR(logger,"CRC failed" << "\n");
      retVal = false;
      break;
    case 1:
      ERROR(logger,"Sensor offline" << "\n");
      retVal = false;
      break;
    default:
      break;
  }
  return retVal;
}

void readSoilMoisture() {
  uint16_t tempAnalogValue = 0;
  tempAnalogValue = analogRead(MOIST_SENS1_PIN);
  delay(10);
  tempAnalogValue = analogRead(MOIST_SENS1_PIN);
  DEBUG(logger, "moist: " << tempAnalogValue << "\n");
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

void readValues(unsigned long currentMillis, bool forced)
{
  if (state == STATE_SETUP || (((currentMillis-lastTimeReadValues) < READINTERVAL) && !forced)){
    return;
  }
  DEBUG(logger, "Reading values...\n");
  lastTimeReadValues = millis();

  //get dallas values
  sensors.requestTemperatures();

  if (true == read_from_am2320()) {
    if (isnan(th.h) || isnan(th.t)) {
      ERROR(logger, "Invalid values from AM2320\n");
    } else {
      humSensorValue = th.h;
      tempSensorValue = th.t;
    }
  } else {
    ERROR(logger, "Failed to get values from AM2320\n");
  }
  DEBUG(logger, "HUM: " << humSensorValue << "\n");
  DEBUG(logger, "TEMP: "<< tempSensorValue << "\n");

  readSoilMoisture();
  DEBUG(logger, "SOIL: " << moisture << "\n");
  delay(10);

  float tempCaseTemp = sensors.getTempCByIndex(0);
  float tempOutsTemp = sensors.getTempCByIndex(1);
  if (isnan(tempCaseTemp) || isnan(tempOutsTemp)) {
    ERROR(logger, "Could not get dallas temperature\n");
  } else {
    tempCase = tempCaseTemp;
    tempOutside = tempOutsTemp;
  }
  DEBUG(logger,"tempOut: "<< tempOutsTemp << "\n");
  DEBUG(logger,"tempCase: "<< tempCaseTemp << "\n");

  lightSensValue = analogRead(LIGHTSENS_PIN);
  DEBUG(logger, "LightSensor: "<< lightSensValue << "\n");
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
    INFO(logger, "Alert! over " << alertLevel << "degrees, open hatch\n");
    turnServo(90);
    hatchOpen = true;
  }
  if (hatchOpen && ((tempSensorValue <= (alertLevel - 2) && humSensorValue <= (humLevel - 2)) || tempSensorValue <= 20)) {
    INFO(logger, "Alert! less than " << alertLevel << "(" << tempSensorValue <<") degrees, close hatch\n");
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
  readConfigSensors();
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

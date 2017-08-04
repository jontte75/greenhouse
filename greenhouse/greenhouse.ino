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
#include "./greenhouse_log.h"

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
#define I2C_INDICATORLED 13
#define AM_SCL           14
#define AM_SDA           15


//I2C related
#define I2C_SLAVEID           0x10
#define I2C_CMD_HELLO         0x00
#define I2C_CMD_GET_DATA_SET1 0x01
#define I2C_CMD_GET_DATA_SET2 0x02
#define I2C_CMD_TOGGLE_WATER  0x0A
#define I2C_CMD_TOGGLE_LIGHT  0x0B
#define I2C_INITIAL_VAL       0xFF
#define I2C_BUFFER_SZ         0x06
#define I2C_BUFFER2_SZ        0x06

// The baud rate of the serial interface
#define SERIAL_BAUD  9600

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
volatile uint8_t tempLimitPotValue = 0; //these values vary, use when set
volatile uint8_t humLimitPotValue = 0;  //these values vary, use when set
uint8_t buttonState = LOW;
uint8_t alertLevel  = 30; //alert level for temp stored from tempLimitPotValue
uint8_t humLevel    = 80; //alert level for humidy stored from humLimitPotValue
uint8_t updateValuesCntr = 0;
unsigned long lastTimeReadValues = 0;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(DALLAS_ONE_WIRE);

// Pass the oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

//initialize the AM2320 library
//Note! Need to use library that supports SoftWire
//otherwise collides with I2C between Omega and Arduino 
//This library also supports multiple AM2320 sensors..
//https://github.com/lazyscheduler/AM2320 
AM2320 th(AM_SCL, AM_SDA);

// Initialize the LCD library with the interface pins
LiquidCrystal595 lcd(LCDPIN1, LCDPIN2, LCDPIN3);
Servo servo1; // Create a servo object

// Some global variables
volatile byte command = I2C_INITIAL_VAL;
volatile uint8_t i2c_ongoing = 0;
volatile static float buffer[I2C_BUFFER_SZ]={I2C_INITIAL_VAL};
volatile static float buffer2[I2C_BUFFER2_SZ]={I2C_INITIAL_VAL};
/*---------------------setup and main loop---------------------*/

void setup() {
  tempSensorValue = humSensorValue = 0;
  SERIAL_BEGIN(SERIAL_BAUD);
  delay(500);
  LOG_DEBUGLN("\nStarting up...");
  lcd.begin(16, 2); // Set the display to 16 columns and 2 rows
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set up I2C.");
  Wire.begin(I2C_SLAVEID);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set up servo.");
  turnServo(180);
  
  analogReference(DEFAULT);
  pinMode(BTNPIN, INPUT);

  pinMode(MOIST_SENS1_PIN, INPUT);
  pinMode(I2C_INDICATORLED, OUTPUT);

  // Initialize the library for temperature sensors
  delay(300);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set up sensors.");
  sensors.begin();
  delay(100);

  //read values, especially potentiometer values, incase of restart
  readValues(0,true);
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

//Request Event callback function, will be called when master reads from I2C bus
void requestEvent() {
  switch (command){
    case I2C_CMD_HELLO:
      Wire.write("Hello!");
      break;
    case I2C_CMD_GET_DATA_SET1:
      Wire.write((byte*)&buffer[0], sizeof(buffer));
      break;
    case I2C_CMD_GET_DATA_SET2:
      Wire.write((byte*)&buffer2[0], sizeof(buffer2));
      break;
    default:
       Wire.write(I2C_INITIAL_VAL);
  }
  i2c_ongoing = 0;
  //digitalWrite(I2C_INDICATORLED, !digitalRead(I2C_INDICATORLED));
}

// Receive Event callback, called when master writes data to I2C bus
void receiveEvent(int bytes)
{
  //digitalWrite(I2C_INDICATORLED, !digitalRead(I2C_INDICATORLED));
  (void)Wire.read();//address
  byte cmd = Wire.read();
  switch (cmd) {
    case I2C_CMD_HELLO:
      command = cmd;
      break;
    case I2C_CMD_GET_DATA_SET1:
      buffer[0] = tempSensorValue;
      buffer[1] = humSensorValue;
      buffer[2] = tempCase;
      buffer[3] = tempOutside;
      buffer[4] = lightSensValue;
      buffer[I2C_BUFFER_SZ-1] = (float)((buffer[0]+buffer[1]+buffer[2]+buffer[3]+buffer[4])/5);
      command = cmd;
      break;
    case I2C_CMD_GET_DATA_SET2:
      buffer2[0] = moisture;
      buffer2[1] = hatchOpen;
      buffer2[2] = 0;
      buffer2[3] = 0;
      buffer2[4] = 0;   
      buffer2[I2C_BUFFER2_SZ-1] = (float)((buffer2[0]+buffer2[1]+buffer2[2]+buffer2[3]+buffer2[4])/5);
      command = cmd;
      break;
    case I2C_CMD_TOGGLE_WATER:
    case I2C_CMD_TOGGLE_LIGHT:
      command = I2C_INITIAL_VAL;
      break;
    default:
      LOG_ERROR("Unexpected command value ");
      LOG_ERRORLN(cmd);
  }
  i2c_ongoing = 1;
}

//The main loop
void loop()
{
  buttonState = digitalRead(BTNPIN); // Check for button press
  lcd.setCursor(0, 0); // Set cursor to home position
  handleState();
  delay(100);
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
      LOG_ERRORLN("CRC failed");
      return false;
      break;
    case 1:
      LOG_ERRORLN("Sensor offline");
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

void readValues(unsigned long currentMillis, bool isInitial)
{
  if (state == STATE_SETUP || (((currentMillis-lastTimeReadValues) < READINTERVAL) && !isInitial)){
    return;
  }
  LOG_DEBUGLN("Reading values...");
  lastTimeReadValues = millis();

  //get dallas values
  sensors.requestTemperatures();

  if (true == read_from_am2320()) {
    if (isnan(th.h) || isnan(th.t)) {
      LOG_ERRORLN("Invalid values from AM2320");
    } else {
      humSensorValue = th.h;
      tempSensorValue = th.t;
    }
  } else {
    LOG_ERROR("Failed to get values from AM2320");
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
  LOG_DEBUG("tp:");
  LOG_DEBUGLN(tempLimitPotValue);
  delay(10);
  readHumidityLimit();
  LOG_DEBUG("hp:");
  LOG_DEBUGLN(humLimitPotValue);

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

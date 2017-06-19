/**
   Greenhouse-project
   - monitor temperature and humidity. If temperature rises too high (or humidity), then open the window
   - monitor also soil humidity
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

//I2C related
#define I2C_INDICATORLED  13
//#define I2C_CLOCKSPEEDHZ  10000L
#define I2C_SLAVEID       0x10
#define I2C_CMD_HELLO     0x00
#define I2C_CMD_REQUEST   0x01
#define I2C_CMD_REQUEST2  0x02
#define I2C_CMD_SET       0x0A
#define I2C_INITIAL_VAL   0xFF
#define I2C_BUFFER_SZ     0x07
#define I2C_BUFFER2_SZ    0x02

// The baud rate of the serial interface
#define SERIAL_BAUD  9600

//global variables
int8_t  maxC = 125, minC = -125;
float   tempSensorValue = 0;
uint8_t tempLimitPotValue = 0;
float   humSensorValue = 0;
uint8_t humLimitPotValue = 0;
uint8_t buttonState = LOW;
uint8_t alertLevel  = 30;
uint8_t humLevel = 80;
boolean hatchOpen = false;
uint8_t updateValuesCntr = 0;
uint8_t moisture = 100;
float  tempCase = 0; //temp inside the gadget
float  tempOutside = 0; //temp outside the greenhouse
uint16_t lightSensValue = 0;


// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(DALLAS_ONE_WIRE);

// Pass the oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

//initialize the AM2320 library
AM2320 th;

// Initialize the LCD library with the interface pins
LiquidCrystal595 lcd(LCDPIN1, LCDPIN2, LCDPIN3);
Servo servo1; // Create a servo object

// Some global variables
volatile byte command = I2C_INITIAL_VAL;
volatile uint8_t i2c_ongoing = 0;
volatile float buffer[I2C_BUFFER_SZ]={1, 2.1, 3.4, -4, 5, 6, I2C_INITIAL_VAL};

/*---------------------setup and main loop---------------------*/

void setup() {
  lcd.begin(16, 2); // Set the display to 16 columns and 2 rows
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setting up I2C.");
  Wire.begin(I2C_SLAVEID);
  //Wire.setClock(I2C_CLOCKSPEEDHZ);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
  delay(5000);
  lcd.clear();
  SERIAL_BEGIN(SERIAL_BAUD);
  delay(500);
  turnServo(180);
  
  LOG_DEBUGLN("\nStarting up...");
  analogReference(DEFAULT);
  pinMode(BTNPIN, INPUT);

  pinMode(MOIST_SENS1_PIN, INPUT);
  pinMode(I2C_INDICATORLED, OUTPUT);

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

//Request Event callback function, will be called when master reads from I2C bus
void requestEvent() {
  //LOG_DEBUG("request, command: ");
  //LOG_DEBUGLN(command);
  //digitalWrite(I2C_INDICATORLED, !digitalRead(I2C_INDICATORLED));
  switch (command){
    case I2C_CMD_HELLO:
      //hello, to check connection is OK
      Wire.write("Hello!");
      break;
    case I2C_CMD_REQUEST:
      //get data
      Wire.write((byte*)&buffer[0], sizeof(buffer));
      //Wire.write("Shit!");
      break;
    case I2C_CMD_SET:
      //we could toggle some led or something else
      Wire.write(I2C_INITIAL_VAL);
      break;
    default:
       Wire.write(I2C_INITIAL_VAL);
  }
  //flush
  while(Wire.available()){
    Wire.read();
  }
  i2c_ongoing = 0;
}

// Receive Event callback, called when master writes data to I2C bus
void receiveEvent(int bytes)
{
  byte addr =  Wire.read();
  byte cmd = Wire.read();
  switch (cmd) {
    case 0:
      command = I2C_CMD_HELLO;
      break;
    case 1:
      command = I2C_CMD_REQUEST;
      break;
    case 2:
      command = I2C_CMD_REQUEST2;
      break;
    default:
      LOG_DEBUG("Unexpected command value ");
      LOG_DEBUGLN(cmd);
  }
  //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  //flush
  while(Wire.available()){
    Wire.read();
  }
  i2c_ongoing = 1;
}

//The main loop
void loop()
{
  lcd.setCursor(0, 0); // Set cursor to home position
  //@todo move reading of values to timer handling
  readValues();
  handleState();

  buffer[0] = tempSensorValue;
  buffer[1] = humSensorValue;
  buffer[2] = tempCase;
  buffer[3] = tempOutside;
  buffer[4] = lightSensValue;
  buffer[5] = hatchOpen;
  buffer[I2C_BUFFER_SZ-1] = (float)((buffer[0]+buffer[1]+buffer[2]+buffer[3]+buffer[4]+buffer[5])/6);
  //buffer2[0] = moisture;
  //buffer2[I2C_BUFFER_SZ-1] = (float)((buffer2[0])/1);

  //delay(100);//this might not be necessary, and disturbs handling of "set" button
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
  if (i2c_ongoing) return;
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

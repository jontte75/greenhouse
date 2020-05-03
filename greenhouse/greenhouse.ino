/**
   Greenhouse-project
   - monitor temperature and humidity. If temperature rises too high (or humidity), then open the window
   - monitor also soil humidity

   Help and ideas have been gathered from many sources, thanx to all!
   E.g from:
   http://www.instructables.com/id/Temperature-with-DS18B20/
   https://learn.adafruit.com/photocells/using-a-photocell
   http://www.instructables.com/id/Connecting-AM2320-With-Arduino/
   http://allaboutee.com/2015/01/02/esp8266-arduino-led-control-from-webpage/
   https://github.com/lazyscheduler/AM2320
   and many other resources
*/

#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/pgmspace.h>
#include "./log.h"
#include <Wire.h>
#include <Sodaq_SHT2x.h>

//analog pin assignments
#define MOIST_SENS1_PIN  A2
#define LIGHTSENS_PIN    A5

//digital pin assignments
#define DALLAS_ONE_WIRE  6
#define SERVOPIN         7
#define ERROR_LED        8
#define DATA_LED         9
#define COMM_LED         10
//Reserved
#define SERIAL_1_RX      19
#define SERIAL_1_TX      18
#define SERIAL_2_RX      17
#define SERIAL_2_TX      16
#define SERIAL_3_RX      15
#define SERIAL_3_TX      14
// reserve following for use of SHT21
// #define SHT_SDA         20
// #define SHT_SCL         21

// The baud rate of the serial interface
#define SERIAL_BAUD  9600
#define LOGGER_BAUD  9600

// How often should we read sensors
#define READINTERVAL (30*1000)

#define LED_OFF LOW
#define LED_ON  HIGH

//global variables sent outside
volatile float   tempSensorValue = 0;
volatile float   humSensorValue = 0;
volatile float   dewPoint = 0;
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
uint8_t alertLevel  = 28; //alert level for temp stored from tempLimitPotValue
uint8_t humLevel    = 80; //alert level for humidy stored from humLimitPotValue
uint8_t updateValuesCntr = 0;
volatile unsigned long lastTimeReadValues = 0;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(DALLAS_ONE_WIRE);

// Pass the oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

Servo servo1; // Create a servo object

// Some global variables
volatile byte command = 0;

Log logger(Serial1, LOGGER_BAUD, LOG_DEBUG2);

template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; }

/*---------------------setup and main loop---------------------*/
void setup()
{
    tempSensorValue = humSensorValue = 0;
    Serial.begin(SERIAL_BAUD);
    uint64_t serialNumber = 0ULL;
    
    delay(500);
    DEBUG(logger, "Starting up...\n");
    turnServo(180);

    analogReference(DEFAULT);
    pinMode(MOIST_SENS1_PIN, INPUT);
    pinMode(ERROR_LED, OUTPUT);
    pinMode(DATA_LED, OUTPUT);
    pinMode(COMM_LED, OUTPUT);

    Wire.begin();

    // Initialize the library for temperature sensors
    delay(300);
    sensors.begin();
    delay(500);

    //read values
    readValues();

    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function below
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    digitalWrite(ERROR_LED, LED_ON);
    digitalWrite(DATA_LED, LED_ON);
    digitalWrite(COMM_LED, LED_ON);
    delay(1000);
    digitalWrite(ERROR_LED, LED_OFF);    
    digitalWrite(DATA_LED, LED_OFF);
    digitalWrite(COMM_LED, LED_OFF);    
}

SIGNAL(TIMER0_COMPA_vect)
{
    unsigned long currentMillis = millis();
    if ((currentMillis - lastTimeReadValues) >= READINTERVAL) {
        readValues();
    }
}

//The main loop
void loop()
{
    handleWindow();
    handleSerialCommunication();
    delay(100);
}

void handleSerialCommunication(){
    while (Serial.available() > 0) {
        digitalWrite(COMM_LED, LED_ON);
        String str = Serial.readString();
        if (str.substring(0) == "hello\r\n") {
            Serial << "Hello world"
                   << "\r\n"
                   << "Humidity(RH): " << humSensorValue
                   << "\r\nTemperature(C): "  << tempSensorValue
                   << "\r\nDewpoint(C): " <<  dewPoint
                   << "\r\n";
        } else if (str.substring(0) == "get\r\n") {
            Serial << tempSensorValue << ":"
                   << humSensorValue << ":"
                   << dewPoint << ":"
                   << tempCase << ":"
                   << tempOutside << ":"
                   << lightSensValue << ":"
                   << moisture << ":"
                   << (hatchOpen ? 1 : 0) << "\r\n";
        } else {
            Serial << "unknown command:" << str << "\r\n";
            digitalWrite(ERROR_LED, LED_ON);
            delay(500);
            digitalWrite(ERROR_LED, LED_OFF);
        }
    }
    delay(100);
    digitalWrite(COMM_LED, LED_OFF);
}

void readSoilMoisture()
{
    uint16_t tempAnalogValue = 0;
    tempAnalogValue = analogRead(MOIST_SENS1_PIN);
    delay(10);
    tempAnalogValue = analogRead(MOIST_SENS1_PIN);
    DEBUG(logger, "moist: " << tempAnalogValue << "\n");
    moisture = uint8_t(100 * (1 - ((double)(tempAnalogValue) / 1023)));
}

void readValues()
{
    digitalWrite(DATA_LED, LED_ON);
    DEBUG(logger, "Reading values...\n");
    lastTimeReadValues = millis();

    //get dallas values
    sensors.requestTemperatures();

    //get values from SHT21 sensor
    humSensorValue = SHT2x.GetHumidity();
    tempSensorValue = SHT2x.GetTemperature();
    dewPoint = SHT2x.GetDewPoint();

    DEBUG(logger, "HUM: " << humSensorValue << "\n");
    DEBUG(logger, "TEMP: " << tempSensorValue << "\n");
    DEBUG(logger, "Dew point: " << dewPoint << "\n");

    readSoilMoisture();
    DEBUG(logger, "SOIL: " << moisture << "\n");
    delay(10);

    float tempCaseTemp = sensors.getTempCByIndex(0);
    float tempOutsTemp = sensors.getTempCByIndex(1);
    if (isnan(tempCaseTemp) || isnan(tempOutsTemp)) {
        ERROR(logger, "Could not get dallas temperature\n");
        digitalWrite(ERROR_LED, LED_ON);
    } else {
        digitalWrite(ERROR_LED, LED_OFF);
        tempCase = tempCaseTemp;
        tempOutside = tempOutsTemp;
    }
    DEBUG(logger, "tempOut: " << tempOutsTemp << "\n");
    DEBUG(logger, "tempCase: " << tempCaseTemp << "\n");

    lightSensValue = analogRead(LIGHTSENS_PIN);
    DEBUG(logger, "LightSensor: " << lightSensValue << "\n");
    delay(100);
    digitalWrite(DATA_LED, LED_OFF);
}

void turnServo(uint16_t degree)
{
    servo1.attach(SERVOPIN); // Attaches the servo on pin 14 to the servo1 object
    servo1.write(degree);
    delay(3000);
    servo1.detach(); //detach not to disturb SoftwareSerial
}

void handleWindow()
{
    if (!hatchOpen && (tempSensorValue >= alertLevel || (humSensorValue >= humLevel && tempSensorValue >= 20))) {
        INFO(logger, "Alert! over " << alertLevel << "degrees, open hatch\n");
        turnServo(90);
        hatchOpen = true;
    }
    if (hatchOpen && ((tempSensorValue <= (alertLevel - 2) && humSensorValue <= (humLevel - 2)) || tempSensorValue <= 20)) {
        INFO(logger, "Alert! less than " << alertLevel << "(" << tempSensorValue << ") degrees, close hatch\n");
        turnServo(180);
        hatchOpen = false;
    }
}

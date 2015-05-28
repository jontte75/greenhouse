#include <EtherCard.h>

#include <LiquidCrystal595.h>
#include <Dht11.h>
#include <Servo.h>


int maxC=0, minC=100;
const byte buttonPin = 5;
int tempSensorValue = 0;
int tempLimitPotValue = 0;
int humSensorValue = 0;
int humLimitPotValue = 0;
int buttonState = LOW;
int alertLevel  = 30;
int humLevel = 80;
boolean hatchOpen = false;

//state handling
const byte STATE_INITIAL = 0;
const byte STATE_WORKING = 1;
const byte STATE_SETUP   = 2;
byte state = STATE_INITIAL;

//analog pin assignments
const byte tempSensorPin = 0;
const byte tempPotPin    = 0;
const byte humPotPin     = 1;
// The data I/O pin connected to the DHT11 sensor
const byte DHT_DATA_PIN = 6;
// The baud rate of the serial interface
const int SERIAL_BAUD = 9600;
// The delay between sensor polls.
const byte POLL_DELAY = 2000;
//digital pin assignments
const byte servo1Pin     = 10;

// Initialize the library with the numbers of the interface pins
//LiquidCrystal lcd(12, 11, 5, 4, 3, 2); // Create an lcd object and assign the pins
//LiquidCrystal lcd(9,8,7,6,5,4); // Create an lcd object and assign the pins, pro mini
LiquidCrystal595 lcd(2,3,4);
Servo servo1; // Create a servo object
Dht11 sensor(DHT_DATA_PIN); //Create 


// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

byte Ethernet::buffer[500]; // tcp/ip send and receive buffer
BufferFiller bfill;

//functions
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(3000);
  Serial.println("\nStarting up...");
  Serial.print("Dht11 Lib version ");
  Serial.println(Dht11::VERSION);
  lcd.begin(16, 2); // Set the display to 16 columns and 2 rows
  analogReference(INTERNAL);
  pinMode(buttonPin, INPUT);
  lcd.clear();

  servo1.attach(servo1Pin); // Attaches the servo on pin 14 to the servo1 object
  servo1.write(180);  // Put servo1 at home position

  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println( "Failed to access Ethernet controller");

  if (!ether.dhcpSetup())
    Serial.println("DHCP failed");

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);
  state = STATE_INITIAL;
}

static word homePage() {
  
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<meta http-equiv='refresh' content='1'/>"
    "<title>Korhonen Greenhouse</title>"
    "<h1>Vilikkaankujan kasvihuone</h1>"
    "<p>L&auml;p&ouml;tila:$D&deg;C</p>"
    "<p>Kosteus:$D&#37;</p>"
    "<p><b>Ikkuna on $S</b></p>"),
      tempSensorValue, humSensorValue,
      (hatchOpen)?"Auki":"Kiinni");
  return bfill.position();
}

void loop() 
{
  lcd.setCursor(0,0); // Set cursor to home position
  readValues();
  handleState();

  if (ether.packetLoop(ether.packetReceive()))  // check if valid tcp data is received
    ether.httpServerReply(homePage()); // send web page data
  delay(250); 
}

void handleState()
{
  if (buttonState == HIGH) { // Change scale state if pressed
    Serial.print("button pressed, state is");
    Serial.println(state);
    if (state == STATE_INITIAL || state == STATE_WORKING){
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
  
  if (state == STATE_SETUP){
    stateSetup();  
  }
  if (state == STATE_WORKING||state == STATE_INITIAL){
    stateWorking();
  }
}

void readValues()
{
    switch (sensor.read()) {
    case Dht11::OK:
      humSensorValue = sensor.getHumidity();
      tempSensorValue = sensor.getTemperature();
      Serial.print("\nTemperature (C): ");
      Serial.print(tempSensorValue);
      Serial.print("\nHumidity (%): ");
      Serial.print(humSensorValue);
      Serial.println();
      break;
    case Dht11::ERROR_CHECKSUM:
      Serial.println("Checksum error");
      break;
    case Dht11::ERROR_TIMEOUT:
      Serial.println("Timeout error");
      break;
    default:
      Serial.println("Unknown error");
      break;
  }

  delay(10);
  tempLimitPotValue = analogRead(tempPotPin); // Read the pot value
  delay(10);
  tempLimitPotValue = analogRead(tempPotPin); // Read the pot value
  tempLimitPotValue = map(tempLimitPotValue, 0, 1023, 20, 40); // Map the values from 20 to 40 degrees
  Serial.print("tp:");
  Serial.println(tempLimitPotValue);

  delay(10);
  humLimitPotValue = analogRead(humPotPin); // Read the pot value
  delay(10);
  humLimitPotValue = analogRead(humPotPin); // Read the pot value
  humLimitPotValue = map(humLimitPotValue, 0, 1023, 20, 100); // Map the values from 20 to 100%
  Serial.print("hp:");
  Serial.println(humLimitPotValue);

  buttonState = digitalRead(buttonPin); // Check for button press  
}

void stateWorking()
{
  //deg_cels = (tempSensorValue * 0.09765625);
  celsius(tempSensorValue);

  if (!hatchOpen && tempSensorValue >= alertLevel){
    Serial.print("Alert! over ");
    Serial.print(alertLevel);
    Serial.print("degrees, open hatch\n");
    servo1.write(90);
    hatchOpen=true;
  }
  if (hatchOpen && tempSensorValue <= alertLevel-2){
    Serial.print("Alert! less than ");
    Serial.print(alertLevel);
    Serial.print("(");
    Serial.print(tempSensorValue);
    Serial.print(") degrees, close hatch\n");
    servo1.write(180);
    hatchOpen=false;
  }  
}

void celsius(float deg_cels) {
  lcd.setCursor(0,0);
  int temp = (int)deg_cels; // Convert to C
  lcd.print(temp);
  lcd.write(B11011111); // Degree symbol
  lcd.print("C ");
  lcd.print("HUM=");
  lcd.print(humSensorValue);
  lcd.print("\%");
  
  if (temp>maxC) {
    maxC=temp;
  }
  if (temp<minC) {
    minC=temp;
  }
  lcd.setCursor(0,1);
  lcd.print("H=");
  lcd.print(maxC);
  lcd.write(B11011111);
  lcd.print("C L=");
  lcd.print(minC);
  lcd.write(B11011111);
  lcd.print("C ");
}

void displaySetup() {
  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print(tempLimitPotValue);
  lcd.print("(");
  lcd.print(alertLevel);
  lcd.print(")          ");
  lcd.setCursor(0,1);
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





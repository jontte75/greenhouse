#include <SoftwareSerial.h>
#include <LiquidCrystal595.h>
#include <Dht11.h>
#include <Servo.h>


int8_t maxC=0, minC=100;
const byte buttonPin = 5;
int8_t tempSensorValue = 0;
uint8_t tempLimitPotValue = 0;
uint8_t humSensorValue = 0;
uint8_t humLimitPotValue = 0;
uint8_t buttonState = LOW;
uint8_t alertLevel  = 30;
uint8_t humLevel = 80;
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
//digital pin assignments
const byte servo1Pin     = 10;
const byte wifiResetPin  = 13;

// The baud rate of the serial interface
const uint16_t SERIAL_BAUD  = 9600;
const uint16_t ESP8266_BAUD = 9600;


// Initialize the library with the numbers of the interface pins
LiquidCrystal595 lcd(2,3,4);
Servo servo1; // Create a servo object
Dht11 sensor(DHT_DATA_PIN); //Create 

//wifi esp8266
SoftwareSerial espSerial(12, 11); // TX, RX

#define BUFFER_SIZE 50
char buffer[BUFFER_SIZE];

uint16_t staticContLen = 0;
#define HOME_PAGE_STATIC_ARR_SZ  9
const uint8_t CONTENT_LEN_PLACE = 2; 
String homePageArray[HOME_PAGE_STATIC_ARR_SZ]={
  "HTTP/1.1 200 OK\r\n",
  "Connection: close\r\n",
//  "Refresh: 120\r\n",
//  "Content-Length:",
  "Content-Type: text/html; charset=UTF-8\r\n\r\n",
  "<!DOCTYPE html PUBLIC>\r\n",
//  "\"-//W3C//DTD XHTML 1.0 Strict//EN\"\r\n",
//  "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n",
  "<html>\r\n",
//  "<head>\r\n",
//  "<title>Vilikkaankujan Kasvihuone</title>\r\n",
//  "</head>\r\n",
  "<body>\r\n",
  "<h1>Kasvihuone</h1>\r\n",
  "</body>\r\n",
  "</html>\r\n\r\n",
};

//functions

String GetResponse(String AT_Command, int wait, char *waitFor=NULL) {
  String tmpData="";
  String dbgData="";
  byte cntr=0;
resend:
  espSerial.println(AT_Command);
  delay(10);
  while (espSerial.available() > 0 )  {
    char c = espSerial.read();
    tmpData += c;

    if ( tmpData.indexOf(AT_Command) > -1 )
      tmpData = "";
    else
      tmpData.trim();
  }
  dbgData=tmpData;
  if (tmpData.indexOf("busy") > -1 ){
      delay(100);
      dbgData+="...busy...\n";
      tmpData="";
      if (++cntr < 10) goto resend;
 }
 delay(wait);
 return dbgData;
}

void sendCommand ( char * cmd,  uint16_t wait, bool cipsend=false ) {
  Serial.println(cmd);
  Serial.println(GetResponse(cmd, wait));
}

void setupWifi()
{
  //connect wifi
  Serial.println("Restarting ESP");
  digitalWrite (wifiResetPin, LOW);
  delay(10000);
  digitalWrite (wifiResetPin, HIGH);
  delay(10000);
  clearSerialBuffer();
  sendCommand ("AT+CIOBAUD=9600", 1000);
  sendCommand ("AT+RST", 8000);
  delay(10000);
  sendCommand ( "AT+CWMODE=1", 2000 );
  sendCommand ( "AT+CWJAP=\"SSID\",\"passwd\"", 10000);
  sendCommand ( "AT+CIPMUX=1", 2000 );
  sendCommand ( "AT+CIPSERVER=1,8080\r", 8000 );
  sendCommand ( "AT+CIPSTO=120\r", 2000 );
  sendCommand ( "AT+CIPSTO?\r", 2000 );
  sendCommand ( "AT+CIFSR\r", 3000);
  sendCommand ( "AT+GMR\r", 3000);
}

void setup() {
  uint8_t cntr;
  for (cntr=3;cntr < HOME_PAGE_STATIC_ARR_SZ;cntr++){
    staticContLen+=homePageArray[cntr].length();
  }

  Serial.begin(SERIAL_BAUD);

  servo1.attach(servo1Pin); // Attaches the servo on pin 14 to the servo1 object
  servo1.write(180);  // Put servo1 at home position
  delay(1000);
  servo1.detach();

  // Open serial communications and wait for port to open:
  espSerial.begin(9600);
  espSerial.setTimeout(15000);
  delay(3000);
  Serial.println("\nStarting up...");
  Serial.print("Dht11 Lib version ");
  Serial.println(Dht11::VERSION);
  lcd.begin(16, 2); // Set the display to 16 columns and 2 rows
  analogReference(INTERNAL);
  pinMode(buttonPin, INPUT);
  pinMode(wifiResetPin, OUTPUT);
  digitalWrite (wifiResetPin, HIGH);
  
  lcd.clear();
  lcd.setCursor(0,0);
  
  
  lcd.print("Setting up Wifi.");
  setupWifi();
  
  lcd.setCursor(0,0);
  lcd.print("Wifi is set up.");
  delay(1000);
  lcd.clear();
  
  state = STATE_INITIAL;
}

void loop() 
{
  uint8_t ch_id=0;
  uint16_t packet_len;
  char *pb;

  lcd.setCursor(0,0); // Set cursor to home position
  readValues();
  handleState();
  if(espSerial.available()) // check if the esp is sending a message 
  {
    espSerial.readBytesUntil('\n', buffer, BUFFER_SIZE);
    if (strncmp(buffer, "+IPD,", 5) == 0) {
      // request: +IPD,ch,len:data
      sscanf(buffer + 5, "%d,%d", &ch_id, &packet_len);
      if (packet_len > 0) {
        // read serial until packet_len character received
        // start from :
        pb = buffer + 5;
        while (*pb != ':') pb++;
        pb++;
        if (strncmp(pb, "GET / ", 6) == 0) {
          Serial.println(buffer);
          Serial.print( "get Status from ch:" );
          Serial.println(ch_id);
          delay(100);
          clearSerialBuffer();
          homepage(ch_id);
        }
      }
    }
  }
  clearBuffer();
  delay(500); 
}

void sendToClient(String *data, uint8_t ch_id)
{
  char command[20] = {0};
  snprintf(command,19,"AT+CIPSEND=%d,%d", ch_id, data->length());
  espSerial.println(command);
  if(espSerial.find(">")){
    espSerial.print(*data);
    delay(200);
    if (espSerial.find("OK")){
        while(espSerial.available()) // check if the esp is sending a message 
        {
          char temp=espSerial.read();
          Serial.print(temp);
        }
    }
    else{
      Serial.println("sending data failed...");
    }
  }else{
    Serial.println("D'OH! Connection failure!");
    setupWifi();
  }
  //esp seems wery slow at times, so just in case...
  delay(500);
}

void homepage(uint8_t ch_id) {
  //create dynamic content first, so that we get the length.
  String tempstr = "<p>L&auml;mp&ouml;tila:";
  tempstr += tempSensorValue;
  tempstr += "&deg;C</p>\r\n";
  String humstr = "<p>Kosteus:";
  humstr += humSensorValue;
  humstr += "&#37;</p>\r\n";
  String windowstr = "<p><b>Ikkuna on ";
  windowstr += ((hatchOpen)?"Auki</b></p>\r\n":"Kiinni</b></p>\r\n");
  //count dynamic lengts together and Set Content Len in header
  String hdrContLen = "Content-Length: ";
  hdrContLen += String((tempstr.length() +
                        humstr.length() +
                        windowstr.length()+
                        staticContLen), DEC);
  hdrContLen += "\r\n";
  
  Serial.println("Create HomePage");
  Serial.println(hdrContLen);
  sendToClient(&homePageArray[0], ch_id);
  sendToClient(&homePageArray[1], ch_id);
  sendToClient(&hdrContLen, ch_id);
  sendToClient(&homePageArray[2], ch_id);
  sendToClient(&homePageArray[3], ch_id);
  sendToClient(&homePageArray[4], ch_id);
  sendToClient(&homePageArray[5], ch_id);
  sendToClient(&homePageArray[6], ch_id);
  /*
  sendToClient(&homePageArray[7], ch_id);
  sendToClient(&homePageArray[8], ch_id);
  sendToClient(&homePageArray[9], ch_id);
  sendToClient(&homePageArray[10], ch_id);
  sendToClient(&homePageArray[11], ch_id);
  sendToClient(&homePageArray[12], ch_id);
  sendToClient(&homePageArray[13], ch_id);*/
  //dynamic part
  sendToClient(&tempstr, ch_id);
  sendToClient(&humstr, ch_id);
  sendToClient(&windowstr, ch_id);
  //end of html page
  sendToClient(&homePageArray[7], ch_id);
  sendToClient(&homePageArray[8], ch_id);
  
  Serial.println("HomePage End.");
}

void clearSerialBuffer(void) {
  while ( espSerial.available() > 0 ) {
     espSerial.read();
  }
}

void clearBuffer(void) {
  for (int i = 0; i < BUFFER_SIZE; i++ ) {
    buffer[i] = 0;
  }
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

void turnServo(uint16_t degree)
{
  servo1.attach(servo1Pin); // Attaches the servo on pin 14 to the servo1 object
  servo1.write(degree);  // Put servo1 at home position
  delay(3000);
  servo1.detach();  //detach not to disturb SoftwareSerial
}

void stateWorking()
{
  celsius(tempSensorValue);

  if (!hatchOpen && (tempSensorValue >= alertLevel ||( humSensorValue >= humLevel && tempSensorValue >= 20))){
    Serial.print("Alert! over ");
    Serial.print(alertLevel);
    Serial.print("degrees, open hatch\n");
    turnServo(90);
    hatchOpen=true;
  }
  if (hatchOpen && tempSensorValue <= alertLevel-2 && (humSensorValue <= humLevel) || tempSensorValue <= 20){
    Serial.print("Alert! less than ");
    Serial.print(alertLevel);
    Serial.print("(");
    Serial.print(tempSensorValue);
    Serial.print(") degrees, close hatch\n");
    turnServo(180);
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


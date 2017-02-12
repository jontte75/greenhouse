// Simple I2C slave for testing I2C
// Modified example by  Nicholas Zambetti <http://www.zambetti.com> 
#include <Wire.h>

// Some definitions to be used
#define I2C_INDICATORLED  LED_BUILTIN
#define I2C_CLOCKSPEEDHZ  3400000
#define I2C_SLAVEID       0x10
#define I2C_CMD_HELLO     0x00
#define I2C_CMD_REQUEST   0x01
#define I2C_CMD_SET       0x02
#define I2C_INITIAL_VAL   0xFF
#define I2C_BUFFER_SZ     0x07
#define SERIAL_BAUDRATE   9600

// Some global variables
byte command = I2C_INITIAL_VAL;
uint8_t buffer[I2C_BUFFER_SZ]={1, 2, 3, (uint8_t)(-4), 5, 6, I2C_INITIAL_VAL};

void setup() {
  pinMode(I2C_INDICATORLED, OUTPUT);
  digitalWrite(I2C_INDICATORLED, LOW);
  Wire.begin(I2C_SLAVEID);
  Wire.setClock(I2C_CLOCKSPEEDHZ);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
  Serial.begin(SERIAL_BAUDRATE);
}

void loop() {
  // count the simple checksum byte
  // in real life, buffer will be filled with correct values in this main loop
  // the buffer will be initialized with values so that master will recognize 
  // invalid/initial values.
  buffer[I2C_BUFFER_SZ-1] = (uint8_t)((buffer[0]+buffer[1]+buffer[2]+buffer[3]+buffer[4]+buffer[5])/6);
  delay(100);
}

//Request Event callback function, will be called when master reads from I2C bus
void requestEvent() {
  Serial.print("request, command: "); Serial.println(command, HEX);
  digitalWrite(I2C_INDICATORLED, !digitalRead(I2C_INDICATORLED));
  switch (command){
    case I2C_CMD_HELLO:
      //hello, to check connection is OK
      Wire.write("Hello!");
      break;
    case I2C_CMD_REQUEST:
      //get data
      Wire.write(&buffer[0],sizeof(buffer));
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
      command = I2C_CMD_SET;
      break;
    default:
      Serial.print("error, unexpected command value ");
      Serial.println(cmd);
  }
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  //flush
  while(Wire.available()){
    Wire.read();
  }
}

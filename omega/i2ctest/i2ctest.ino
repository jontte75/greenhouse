// Modified example by  Nicholas Zambetti <http://www.zambetti.com> 
#include <Wire.h>

byte command = 0xff;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Wire.begin(0x10);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
  Serial.begin(9600);
}

void loop() {
  delay(100);
}

// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent() {
  static byte buffer[6]={1,2,3,-4,5,6};
  Serial.print("request, command: "); Serial.println(command, HEX);

  switch (command){
    case 0x00:
      //hello, to check connection is OK
      Wire.write("Hello!",6);
      break;
    case 0x01:
      //get data
      Wire.write(buffer,6);
      break;
    case 0xff:
      //initial value, just blink
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      Wire.write(0xfe);
      break;
    default:
       Wire.write(19);
  }
  //command = 0xff;
}

void receiveEvent(int bytes)
{
  byte addr =  Wire.read();
  byte cmd = Wire.read();
  switch (cmd) {
    case 0: // i2cset -y 0 0x08 0x00
      digitalWrite(LED_BUILTIN, HIGH);
      //Serial.print("toggled led\n");
      command = 0x00;
      break;
    case 1:
      command = 0x01;
      break;
    case 255:
      break;
    default:
      Serial.print("error, unexpected command value ");
      Serial.print(cmd);
  }
}

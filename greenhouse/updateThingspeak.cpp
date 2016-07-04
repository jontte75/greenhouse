
#include "updateThingspeak.h"


#define MAX_APN_LEN    20
#define MAX_PWD_LEN    20
#define MAX_IP_STR_LEN 16
#define MAX_TS_KEY_LEN 20
#define SUCCESS 1
#define FAILURE 0
#define BUFFER_SIZE 145

UpdateThingspeak::UpdateThingspeak(short rxPin,
                                   short txPin,
                                   short baud,
                                   uint8_t rstPin,
                                   const char *apn,
                                   const char* pwd,
                                   const char* tsip,
                                   const char* tswkey) {
  this->espSerial = new SoftwareSerial(rxPin, txPin);
  this->rstPin = rstPin;

  memset(buffer, 0, sizeof(buffer));
  memset(this->wifi_apn, 0, sizeof(this->wifi_apn));
  strncpy(this->wifi_apn, apn, MAX_APN_LEN);
  memset(this->wifi_pwd, 0, sizeof(this->wifi_pwd));
  strncpy(this->wifi_pwd, pwd, MAX_PWD_LEN);
  memset(this->tsip, 0, sizeof(this->tsip));
  strncpy(this->tsip, tsip, MAX_IP_STR_LEN);
  memset(this->tswkey, 0, sizeof(this->tswkey));
  strncpy(this->tswkey, tswkey, MAX_TS_KEY_LEN);
  //set esp pin mode
  
  pinMode(this->rstPin, OUTPUT);
  digitalWrite (this->rstPin, HIGH);
  // Open serial communications and wait for port to open:
  this->espSerial->begin(baud);
  this->espSerial->setTimeout(15000);
};

void UpdateThingspeak::sendCommand ( const char * cmd,  uint16_t wait) {
  String tmpData = "";
  byte cntr = 0;
  LOG_DEBUGLN(cmd);
resend:
  this->espSerial->println(cmd);
  delay(10);
  while (this->espSerial->available() > 0 )  {
    char c = this->espSerial->read();
    tmpData += c;

    if ( tmpData.indexOf(cmd) > -1 )
      tmpData = "";
    else
      tmpData.trim();
  }
  if (tmpData.indexOf("busy") > -1 ) {
    LOG_DEBUGLN("busy");
    delay(100);
    tmpData = "";
    if (++cntr < 10) goto resend;
  }
  LOG_DEBUGLN(tmpData);
  delay(wait);
}

void UpdateThingspeak::clearSerialBuffer(void) {
  while ( this->espSerial->available() > 0 ) {
    this->espSerial->read();
  }
}

void UpdateThingspeak::clearBuffer(void) {
  for (int i = 0; i < BUFFER_SIZE; i++ ) {
    this->buffer[i] = 0;
  }
}


uint8_t UpdateThingspeak::setWifiPwd(const char* pwd) {
  if (!pwd) return FAILURE;
  strncpy(this->wifi_pwd, pwd, MAX_PWD_LEN);
  return SUCCESS;
}

uint8_t UpdateThingspeak::setWifiApn(const char* apn) {
  if (!apn) return FAILURE;
  strncpy(this->wifi_apn, apn, MAX_APN_LEN);
  return SUCCESS;
}

void UpdateThingspeak::setupWifi()
{
  LOG_DEBUGLN("Restarting ESP");
  digitalWrite (this->rstPin, LOW);
  delay(8000);
  digitalWrite (this->rstPin, HIGH);
  delay(10000);
  this->clearSerialBuffer();
  this->sendCommand ("AT+CIOBAUD=9600", 1000);
  this->sendCommand ("AT+RST", 8000);
  this->sendCommand ("AT+CWMODE=1", 2000 );
  snprintf(buffer, BUFFER_SIZE - 1, "AT+CWJAP=\"%s\",\"%s\"",
           this->wifi_apn,
           this->wifi_pwd);
  this->sendCommand (buffer, 15000);
  memset(buffer, 0, sizeof(buffer));
  this->sendCommand ("AT+CIFSR\r", 3000);
  this->sendCommand ("AT+GMR\r", 3000);
}

uint8_t UpdateThingspeak::checkIsEspResponsive(void)
{
    this->espSerial->println("AT+GMR\r");
    if (this->espSerial->find((char*)"OK")) {
      LOG_DEBUGLN("Wifi OK");
      return SUCCESS;
    } else {
      LOG_DEBUGLN("Wifi NOK, restart WIfi");
    }
  return FAILURE;
}


void UpdateThingspeak::updateValues(float field1, float field2, uint8_t field3, float field4, float field5, float field6, uint16_t field7)
{
  char cmd[60] = {0};
  char float_str[10] = {0};
  char hum_str[10] = {0};
  char moist_str[10] = {0};
  char out_str[10] = {0};
  char case_str[10] = {0};


  snprintf(cmd, 59, "AT+CIPSTART=\"TCP\",\"%s\",80", this->tsip);
  this->espSerial->println(cmd);
  delay(2000);
  if (this->espSerial->find((char*)"Error")) {
    LOG_DEBUG("Error1");
    return;
  }
  snprintf(this->buffer, BUFFER_SIZE - 1, "GET /update?key=%s&field1=%s&field2=%s&field3=%u&field4=%s&field5=%s&field6=%s&field7=%u\r\n",
           this->tswkey,
           dtostrf(field1, 4, 2, float_str),
           dtostrf(field2, 4, 2, hum_str),
           (field3) ? 1 : 0,
           dtostrf(field4, 4, 2, moist_str),
           dtostrf(field5, 4, 2, out_str),
           dtostrf(field6, 4, 2, case_str),
           field7);

  LOG_DEBUG(this->buffer);
  this->espSerial->print("AT+CIPSEND=");
  this->espSerial->println(strlen(buffer));

  if (this->espSerial->find((char*)">")) {
    this->espSerial->print(buffer);
  }
  else {
    this->espSerial->println("AT+CIPCLOSE");
  }
}

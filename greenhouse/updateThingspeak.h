#ifndef __UPDATETHINGSPEAK_H__
#define __UPDATETHINGSPEAK_H__

#include "Arduino.h"
#include "greenhouse_log.h"
#include <SoftwareSerial.h>


#define MAX_APN_LEN    20
#define MAX_PWD_LEN    20
#define MAX_IP_STR_LEN 16
#define MAX_TS_KEY_LEN 20
#define SUCCESS 1
#define FAILURE 0
#define BUFFER_SIZE 145

class UpdateThingspeak
{
  private:
    SoftwareSerial *espSerial;
    char wifi_apn[MAX_APN_LEN + 1];
    char wifi_pwd[MAX_PWD_LEN + 1];
    char tsip[MAX_IP_STR_LEN + 1];
    char tswkey[MAX_TS_KEY_LEN + 1];
    uint8_t rstPin;
    char buffer[BUFFER_SIZE];

    void sendCommand ( const char * cmd,  uint16_t wait);
    void clearSerialBuffer(void);
    void clearBuffer(void);

  public:
    UpdateThingspeak(short rxPin,
                     short txPin,
                     short baud,
                     uint8_t rstPin,
                     const char *apn,
                     const char* pwd,
                     const char* tsip,
                     const char* tswkey);

    uint8_t setWifiPwd(const char* pwd);
    uint8_t setWifiApn(const char* apn);
    void setupWifi();
    uint8_t checkIsEspResponsive(void);
    void updateValues(float field1, float field2, uint8_t field3, float field4, float field5, float field6, uint16_t field7);

};

#endif

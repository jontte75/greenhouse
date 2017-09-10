#ifndef _I2CCOM_H
#define _I2CCOM_H
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <iostream>
#include <cmath>
#include "onion-i2c.h"

namespace i2cc{

class I2cCom{
private:
    uint8_t buffer[I2C_BUFFER_SIZE];
    int     devNum;
    int     devAddr;
    int     addr;
    bool    connected;
public:
    static const uint8_t I2C_CMD_HELLO         = 0x00;
    static const uint8_t I2C_CMD_GET_DATA_SET1 = 0x01;
    static const uint8_t I2C_CMD_GET_DATA_SET2 = 0x02;
    static const uint8_t I2C_CMD_MAX           = 0x03;
    static const uint8_t I2C_DEF_BYTES         = 25;
    I2cCom();
    ~I2cCom() {};
    bool connect();
    bool storeDataFromI2c(uint8_t cmd);
    const uint8_t* getData();
};
} //namespace
#endif
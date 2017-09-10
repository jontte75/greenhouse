#ifndef _I2CDATA_H
#define _I2CDATA_H

#include <iterator>
#include <iostream>
#include <algorithm>
#include <array>
#include "onion-i2c.h"

namespace i2cd{

#define I2C_MIN_SZ          9 //1+4+4 (addr+value+checksum)
#define I2C_DATA_ARRAY_SZ   5

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Data recieved from arduino via I2C
// Format on string:
// byte address (not used for now)
// float value1-5
// float checksum (simple values added divided with amount)
// ==> 25 bytes (float is assumed to be 4 bytes) 
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class I2cData{
private:
    uint8_t _index;
    uint8_t address;
    std::array<float, I2C_DATA_ARRAY_SZ> values;
    float   checksum;

    float bytesToFloat(const uint8_t *buff, int *ind=NULL); 
    bool validate();
public:
    I2cData();
    I2cData(uint8_t ind);
    ~I2cData() {};
    bool setData(const uint8_t* data, int sz=I2C_BUFFER_SIZE);
    friend std::ostream &operator<<( std::ostream &output, const I2cData &data );
};

} //namespace
#endif
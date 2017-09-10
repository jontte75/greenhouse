/*
 * =====================================================================================
 *
 *       Filename:  i2cdata.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  27.08.2017 11.17.53
 *       Revision:  none
 *       Compiler:  g++
 *
 *         Author:  Joni Korhonen 
 *   Organization:  
 *
 * =====================================================================================
 */
#include "i2cdata.h"

namespace i2cd{

I2cData::I2cData():_index(1), address(0), checksum(0){
    values.fill(0.0);
}

I2cData::I2cData(uint8_t ind):_index(ind), address(0), checksum(0){
    values.fill(0.0);
}

float I2cData::bytesToFloat(const uint8_t *buff, int *ind) 
{ 
    float result = 0;;
    if (ind){
        *ind += sizeof(float);
    }
    if (buff){
        memcpy(&result, buff, sizeof(result));
    }
    return result;
}

bool I2cData::validate(){
    float temp = 0;
    for(const auto& val: values){
        temp += val;
    }
    
    float cntcheck =  temp/values.size();
    if (checksum != cntcheck){
        std::cout << "invalid checksum " << checksum << " vs " << cntcheck << std::endl;
        return false;
    }

    return true;
}

bool I2cData::setData(const uint8_t* data, int _sz){
    bool result=true;
    int dataInd = 0, valInd = 0;
    
    if(!data || _sz < I2C_MIN_SZ || _sz > I2C_BUFFER_SIZE){
        std::cout << "Invalid parameters"<< std::endl;
        return false;
    }
    
    address = data[dataInd++];
    while(dataInd < (_sz - sizeof(float))){
        values[valInd++]=bytesToFloat(&data[dataInd],&dataInd);
    }
    checksum = bytesToFloat(&data[(_sz-sizeof(float))]);

    if(!validate()){
        std::cout << "validation failed" << std::endl;
        values.fill(0.0);
        checksum = 0;
        address = 0;
        result = false;
    }
    return result;
}

std::ostream &operator<<( std::ostream &output, const I2cData &data ) {
    int ind = data._index;
    for(const auto& val: data.values){
        output << "&field"<< ind << "=" << val;
        ind++;
    }
    return output;            
 }
}//namespace
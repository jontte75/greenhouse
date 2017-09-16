/*
 * =====================================================================================
 *
 *       Filename:  i2ccom.cpp
 *
 *    Description:  I2C communication with arduino
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
#include "i2ccom.h"

namespace i2cc{

I2cCom::I2cCom():devNum(0x10), devAddr(0), addr(0), connected(false){
    memset(buffer, 0, sizeof(buffer));
}

// This function checks connection with Hello and sets connection status
bool I2cCom::connect(){
    std::string helloMsg = "Hello!";
    buffer[0] = I2C_CMD_HELLO;
    uint8_t retries=5;
    while(retries){
        (void)i2c_writeBuffer (devAddr, devNum, addr, &buffer[0], 0x01);
        sleep(1);
        memset(buffer, 0, sizeof(buffer));
        (void)i2c_read (devAddr, devNum, addr, &buffer[0], helloMsg.size()+1);
        if (strncmp(helloMsg.c_str(), (char*)&buffer[1], helloMsg.size()) == 0){
            std::cout<<"YES! connection to Arduino succeeded"<<std::endl;
            connected = true;
            break;
        }else{
            std::cout<<"Oh No! recieved some garbage :-("<<std::endl;
            for(int i=0;i < 32;i++){
                printf("%c",buffer[i]);
            }
            std::cout<<std::endl;
            sleep(10);
            connected = false;
            retries--;
        }
    }
    return connected;
}

// This function checks connection with Hello and sets connection status
bool I2cCom::storeDataFromI2c(uint8_t cmd){
    buffer[0] = cmd;
    bool result = true;
    if(connected){
        (void)i2c_writeBuffer (devAddr, devNum, addr, &buffer[0], 0x01);
        sleep(2);
        memset(buffer, 0, sizeof(buffer));
        if ( 0 != i2c_read (0x00, 0x10, 0x00, &buffer[0], 25)){
            result = false;
        }
    }
    return result;
}

const uint8_t* I2cCom::getData(){
    return &buffer[0];
}
}//namespace
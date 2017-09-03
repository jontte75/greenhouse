/*
 * =====================================================================================
 *
 *       Filename:  main.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  20.08.2017 20.52.20
 *       Revision:  none
 *       Compiler:  g++
 *
 *         Author:  Joni Korhonen 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <iostream>
#include <cmath>
#include "onion-i2c.h"
#include "i2cdata.h"
#include "i2ccom.h"

int main(void)
{
    I2cData set1;
    I2cData set2;
    I2cCom conn;
    //connect to arduino (check i2c with simple hello)
    conn.connect();

    int retries=5;
    while(retries){
        conn.storeDataFromI2c(I2C_CMD_GET_DATA_SET1);
        if(!set1.setData(conn.getData(),25))
        {
            std::cout << "Ohno invalid data from i2c" << std::endl;
            retries--;
            sleep(10);
            continue;
        }
        std::cout << "Data recieved:" << set1 <<std::endl;
        break;
    }
    retries=5;
    while(retries){
        conn.storeDataFromI2c(I2C_CMD_GET_DATA_SET2);
        if(!set2.setData(conn.getData(),25))
        {
            std::cout << "Ohno invalid data from i2c" << std::endl;
            retries--;
            sleep(10);
            continue;
        }
        std::cout << "Data recieved:" << set2 <<std::endl;
        break;
    }    
    return 0;
}

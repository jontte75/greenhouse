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
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "onion-i2c.h"
#include "i2cdata.h"
#include "i2ccom.h"

std::mutex mtex;
std::condition_variable cv;
std::queue<I2cData> dataQueue;

bool success=false;

void i2cComThr(int retries) {
    I2cData set1;
    I2cData set2;
    I2cCom conn;
    //connect to arduino (check i2c with simple hello)
    conn.connect();
    
    while(1){
        {
            std::lock_guard<std::mutex> lk(mtex);
            while(retries){
                conn.storeDataFromI2c(I2C_CMD_GET_DATA_SET1);
                if(!set1.setData(conn.getData(),25))
                {
                    std::cout << "Ohno invalid data from i2c" << std::endl;
                    retries--;
                    success = false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                    continue;
                }
                // std::cout << "Data recieved:" << set1 <<std::endl;
                dataQueue.push(set1);
                success = true;
                break;
            }
            retries=5;
            while(retries){
                conn.storeDataFromI2c(I2C_CMD_GET_DATA_SET2);
                if(!set2.setData(conn.getData(),25))
                {
                    std::cout << "Ohno invalid data from i2c" << std::endl;
                    retries--;
                    success = false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
                    continue;
                }
                success = true;
                // std::cout << "Data recieved:" << set2 <<std::endl;
                dataQueue.push(set2);
                break;
            }
        }
        if (success){
            cv.notify_all();
        }else{
            while(!dataQueue.empty()){
                dataQueue.pop();    
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(300));
    }
}

void thingSpeakThr() {
	while (true) {
        //wait requires unique_lock
        std::unique_lock<std::mutex> lk(mtex);
        cv.wait(lk, []{return success || !dataQueue.empty(); });
		while (!dataQueue.empty()) {
                std::cout<<"--------------------"<<std::endl;
                std::cout<<dataQueue.front();
                std::cout<<std::endl;
                std::cout<<"--------------------"<<std::endl;
                dataQueue.pop();
        }
	}
}

int main(void)
{
	std::thread t1(&i2cComThr, 5);
	std::thread t2(&thingSpeakThr);

    t1.join();
    t2.join();
 
    return 0;
}

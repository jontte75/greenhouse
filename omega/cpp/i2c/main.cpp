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
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "onion-i2c.h"
#include "i2cdata.h"
#include "i2ccom.h"
#include "tsclient.h"

std::mutex mtex;
std::condition_variable cv;
std::queue<i2cd::I2cData> dataQueue;

bool success=false;

void i2cComThr(int retries) {
    i2cc::I2cCom conn;
    //connect to arduino (check i2c with simple hello)
    conn.connect();
    
    while(true){
        uint8_t dsind=1;
        for (int i=1;i<i2cc::I2cCom::I2C_CMD_MAX;i++){
            std::lock_guard<std::mutex> lk(mtex);
            i2cd::I2cData dataSet(dsind);
            success = false;
            int retryCnt = retries;
            while(retryCnt){
                conn.storeDataFromI2c(i);
                if(!dataSet.setData(conn.getData(), i2cc::I2cCom::I2C_DEF_BYTES))
                {
                    std::cout << "Ohno invalid data from i2c" << std::endl;
                    retryCnt--;
                    success = false;
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                    continue;
                }
                // std::cout << "Data recieved:" << set1 <<std::endl;
                dataQueue.push(dataSet);
                success = true;
                break;
            }
            if(!success){
                break;
            }
            dsind+=5;
        }
        if (success){
            cv.notify_all();
            success = false;
            std::this_thread::sleep_for(std::chrono::seconds(300));        
        }else{
            //something went wrong, we could not get all data from Arduino
            std::lock_guard<std::mutex> lk(mtex);
            while(!dataQueue.empty()){
                dataQueue.pop();    
            }
            //wait for a while before requesting data again, but not as long as the TS interval is
            std::this_thread::sleep_for(std::chrono::seconds(60));        
        }

    }
}

void thingSpeakThr(std::string apikey) {
    tsc::tsClient tsc(apikey);
	while (true) {
        //wait requires unique_lock
        std::unique_lock<std::mutex> lk(mtex);
        cv.wait(lk, []{return success || !dataQueue.empty(); });
        std::ostringstream data;
        while (!dataQueue.empty()) {
                std::cout<<"--------------------"<<std::endl;
                data << dataQueue.front();
                std::cout<<"--------------------"<<std::endl;
                dataQueue.pop();
        }        
        data << "\r\n";
        tsc.conn("184.106.153.149", 80);
        tsc.send_data(std::string(data.str()));
        std::cout << "status:" << tsc.receive(256);
	}
}

int main(int argc , char *argv[])
{
    if (argc < 2 || argc > 2){
        std::cerr << "Usage: " << argv[0] << "THINGSPEAKWRITEAPIKEY" << std::endl;
        return 1;
    }
    
    std::string apikey(argv[1]);
	std::thread t1(&i2cComThr, 5);
	std::thread t2(&thingSpeakThr, apikey);

    t1.join();
    t2.join();

    std::cout<<"Main done!"<<std::endl;
 
    return 0;
}

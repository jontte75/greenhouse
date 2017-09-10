#include <iostream> 
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "tsclient.h"
namespace tsc
{
    tsClient::tsClient(std::string apikey):sock(-1), port(0), address("")
    {
        tskey = apikey;
    }

    tsClient::~tsClient()
    {
        if (sock){
            close(sock);
        }
    }

    /**
        Connect to a host on a certain port number
    */
    bool tsClient::conn(std::string address , int port)
    {
        //create socket if it is not already created
        if(sock == -1)
        {
            //Create socket
            sock = socket(AF_INET , SOCK_STREAM , 0);
            if (sock == -1)
            {
                perror("Could not create socket");
            }
             
            std::cout<<"Socket created\n";
        }
        
        server.sin_addr.s_addr = inet_addr( address.c_str() );
         
        server.sin_family = AF_INET;
        server.sin_port = htons( port );
         
        //Connect to remote server
        if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
        {
            perror("connect failed. Error");
            return false;
        }
         
        std::cout<<"Connected\n";
        return true;
    }
     
    /**
        Send data to the connected host
    */
    bool tsClient::send_data(std::string data)
    {
        std::ostringstream head;
        head << "POST /update HTTP/1.1\n"
             << "Host: api.thingspeak.com\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\n"
             << "X-THINGSPEAKAPIKEY: " << tskey << "\n"
             << "Content-Type: application/x-www-form-urlencoded\n"
             << "Content-Length: " << data.length() << "\n\n";
        head << data;
        const auto str = head.str();
        std::cout << str << std::endl;
        //Send some data
        if( send(sock , str.data(), str.size(), 0) < 0)
        {
            perror("Send failed : ");
            return false;
        }
        std::cout<<"Data send\n";
         
        return true;
    }
     
    /**
        Receive data from the connected host
    */
    std::string tsClient::receive(int size=512)
    {
        char buffer[size];
        std::string reply;
         
        //Receive a reply from the server
        if( recv(sock , buffer , sizeof(buffer) , 0) < 0)
        {
            puts("recv failed");
        }
         
        close(sock);
        sock = -1;
        reply = buffer;
        return reply;
    }
} //namespace

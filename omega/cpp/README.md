# Omega2 & CPP

This directory contains Omega2 related C/C++ code for different tasks.

## i2c
This folder contains process for communicating with Arduino using i2c. It also sends recieved data to ThingSpeak.

### i2ccom.*
This handles i2c communication with Arduino.

### i2cdata.*
This is used to validate and store data recieved from Arduino.

### tsclient.*
This is used to send data to ThingSpeak

### main.cpp
Main module. Starts two threads. One thread communicates with arduino and the other sends the data to ThingSoeak when all data has been recieved.

### Compilation
One need proper cros-compilation tool-chain (for omega2), or easiest way is to use dockerised compilation environment, like jlcs/omega2-docker-built.

```bash
$CXX $CFLAGS -std=gnu++11 $LDFLAGS -loniondebug -lonioni2c main.cpp i2cdata.cpp i2ccom.cpp tsclient.cpp -o i2ctest
```
Where:

```bash
LDFLAGS=-L/lede/staging_dir/target-mipsel_24kc_musl/usr/lib -L/home/omega/workspace/i2c-exp-driver/lib

CFLAGS=-I/lede/staging_dir/target-mipsel_24kc_musl/usr/include -I/home/omega/workspace/i2c-exp-driver/include

CXX=/lede/staging_dir/toolchain-mipsel_24kc_gcc-5.4.0_musl/bin/mipsel-openwrt-linux-g++
```

Or use the Makefile. Edit the correct paths to libraries and compiler.

### Usage
Start process by hand, or preferably make it to start automatically when Omega boots.

```bash
root@Omega-9D23:~# cat /etc/rc.local 
# Put your custom commands here that should be executed once
# the system init finished. By default this file does nothing.

./root/i2ctest YOURTHINGSPEAKWRITEKEY &

exit 0
root@Omega-9D23:~# 

```

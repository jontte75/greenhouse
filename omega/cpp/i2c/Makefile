#Makefile for i2ctest

LIBS = -pthread -loniondebug -lonioni2c
CC = /lede/staging_dir/toolchain-mipsel_24kc_gcc-5.4.0_musl/bin/mipsel-openwrt-linux-g++
CFLAGS = -g -Wall -std=c++11
LDFLAGS = -L/lede/staging_dir/target-mipsel_24kc_musl/usr/lib -L/home/omega/workspace/i2c-exp-driver/lib
INCLUDES = -I/lede/staging_dir/target-mipsel_24kc_musl/usr/include -I/home/omega/workspace/i2c-exp-driver/include

TARGET = i2ctest

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp))
HEADERS = $(wildcard *.h)

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LDFLAGS) $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)

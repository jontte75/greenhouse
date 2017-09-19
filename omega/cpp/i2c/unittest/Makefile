#Unittests
#preferably run in x86 linux or target
#set GTESTINC and ONIONI2CINC to point where google test is installed
#CXX = /lede/staging_dir/toolchain-mipsel_24kc_gcc-5.4.0_musl/bin/mipsel-openwrt-linux-g++
CXX = g++
CXXFLAGS = -g -Wall -std=c++11
LIBS = -pthread -lgtest -lgtest_main
INCS =\
	-I./\
	-I../\
	-I$(GTESTINC)\
	-I$(ONIONI2CINC)

LDFLAGS =\
	-L$(GTESTINC)/../\

OBJS =\
	../i2cdata.o\
	i2cdataTest.o

testAll: $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCS) $(OBJS) $(LDFLAGS) $(LIBS) -o testAll

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCS) $(LDFLAGS) $(LIBS) -c $< -o $@

clean:
	rm -f testAll *.o testAll.xml
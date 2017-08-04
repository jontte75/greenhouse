#!/usr/bin/env python
import time
import sys 
import array
import struct
import argparse
from OmegaExpansion import onionI2C

def get_float(data, index):
    """get a float value from data array"""
    return struct.unpack('f', "".join(map(chr, data[4*index:(index+1)*4])))[0]

def print_w_header(str_to_be_printed):
    """print a string w/ header """
    print("#"*(len(str_to_be_printed)+6))
    print("# "),
    print(str_to_be_printed),
    print(" #")
    print("#"*(len(str_to_be_printed)+6))

def handle_cmd_line_args():
    """Handle command line options"""
    parser = argparse.ArgumentParser(description='Test i2c connection betwen Arduino and Omega2')
    parser.add_argument('-l','--loop', action="store", dest='loop', default=1, 
                        help='lengt of data send receive loop, default is 1 and zero equals inifinite')
    parser = parser.parse_args()
    parser.loop = int(parser.loop)
    if (parser.loop < 0 or parser.loop > 100):
        print("Invalid loop count, should be between 0..100")
        sys.exit(-1)
    return parser

###########################
#Start the scrip
###########################
cmdLineArgs = handle_cmd_line_args()

print_w_header("Start testing Omega2 I2c connection with Arduino")

i2c = onionI2C.OnionI2C(0)

# set the verbosity
i2c.setVerbosity(0)

devAddr = 0x10
hello = ""

###########################
#Check connection
###########################
print_w_header("Start with basic hello...")
while (1):
    val = i2c.writeByte(devAddr, 0x00, 0x00)

    #take a short break
    time.sleep(1)

    # read value from arduino
    val = i2c.readBytes(devAddr, 0x00, 7)
    addr = val[0]
    value = array.array('B', val[1:]).tostring()
    print('Read returned: '),
    print(val)
    print('addr:'),
    print(addr)
    print('value:'),
    print(value)
    if ("Hello" in value):
        #OK we received the Hello String
        break;
    time.sleep(5)

###########################
#Now get some real data from Arduino
###########################
time.sleep(5)
print_w_header("Now continue to get some data from Arduino/I2C...")
loop_ctrl = 1 if (cmdLineArgs.loop == 0) else (cmdLineArgs.loop)
command = 1
while (loop_ctrl):
    val = i2c.writeByte(devAddr, 0x00, command)

    #take a short break
    time.sleep(1)

    # read value from arduino
    val = i2c.readBytes(devAddr, 0x00, 24+1)
    addr = val[0]
    checksum = get_float(val[1:], 5)
    cnt_checksum = (get_float(val[1:], 0)+
                    get_float(val[1:], 1)+
                    get_float(val[1:], 2)+
                    get_float(val[1:], 3)+
                    get_float(val[1:], 4))/5

    print("--------COMMAND:"),
    print(command),
    print("--------")
    if ((round(checksum, 2) != round(cnt_checksum, 2)) or (int(cnt_checksum) == 0xFF)):
        print("FAILURE: Invalid checksum! value: "),
        print(val)
        print("checksum: "),
        print(checksum)
        #something went wrong, so sleep a little bit longer
        time.sleep(15)
        continue
    else:
        print("SUCCESS: value: "),
        print("%f "%get_float(val[1:], 0)),
        print("%f "%get_float(val[1:], 1)),
        print("%f "%get_float(val[1:], 2)),
        print("%f "%get_float(val[1:], 3)),
        print("%f "%get_float(val[1:], 4))
        print("checksum: "),
        print(checksum)
    #do not update loop_ctrl if infinitive loop is chosen
    loop_ctrl -= 0 if (cmdLineArgs.loop == 0) else 1
    command = command + 1
    if command >2:
        command = 1
        time.sleep(10)
    else:
        time.sleep(1)

print 'Done!'

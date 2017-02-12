import time
import sys 
import array
from OmegaExpansion import onionI2C

def print_w_header(str_to_be_printed):
    """ print a string w/ header """
    #this is not p3 compatible
    print "#"*(len(str_to_be_printed)+6)
    print "# ",
    print str_to_be_printed,
    print " #"
    print "#"*(len(str_to_be_printed)+6)

###########################
#Start the scrip
###########################

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
    time.sleep(2)

    # read value from arduino
    val = i2c.readBytes(devAddr, 0x00, 9)
    addr = val[0]
    value = array.array('B', val[3:]).tostring()
    print('Read returned: ', val)
    print('addr:', addr)
    print('value:', value)
    if ("Hello" in value):
        #OK we received the Hello String
        break;
    time.sleep(5)

###########################
#Now get some real data from Arduino
###########################
time.sleep(5)
print_w_header("Now continue to read data...")
while (1):
    val = i2c.writeByte(devAddr, 0x00, 0x01)

    #take a short break
    time.sleep(2)

    # read value from arduino
    val = i2c.readBytes(devAddr, 0x00, 10)
    addr = val[0]
    value = array.array('h', val[3:])
    checksum = value[6]
    cnt_checksum = (value[0]+value[1]+value[2]+value[3]+value[4]+value[5])/6
    if ((checksum != cnt_checksum) or (checksum == 0xFF)):
        print("FAILURE: Invalid checksum! value: ", val)
        print('checksum: ', checksum)
        #something went wrong, so sleep a little bit longer
        time.sleep(10)
    else:
        print('SUCCESS: value: ', value[:6])
        print("checksum: ", checksum)
        time.sleep(1)

print('Done!')

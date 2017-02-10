from OmegaExpansion import onionI2C
import time
import sys 
import array

print('Start testing Omega2 I2c connection with Arduino')

i2c = onionI2C.OnionI2C(0)

# set the verbosity
i2c.setVerbosity(0)

devAddr = 0x10
hello = ""

ret = raw_input('start i2c test, hit enter')

###########################
#Check connection
###########################
while (1):
    command = 0x00
    print('Writing to device 0x%02x writing: 0x%02x'%(devAddr, command))
    val = i2c.writeByte(devAddr, 0x00, command)

    #take a short break
    time.sleep(1)

    # read value from arduino
    print('Reading from device 0x%02x'%(devAddr))
    val = i2c.readBytes(devAddr, 0x00, 6)
    addr = val[0]
    value = array.array('B',val[1:]).tostring()
    print('Read returned: ', val)
    print('addr:',addr)
    print('value:',value)
    if ("Hello" in value):
        #OK we received the Hello String
        break;
    time.sleep(5)

###########################
#Now get some real data from Arduino
###########################
cntr = 0;
while (cntr < 10):
    command = 0x01
    print('Writing to device 0x%02x writing: 0x%02x'%(devAddr, command))
    val = i2c.writeByte(devAddr, 0x00, command)

    #take a short break
    time.sleep(1)

    # read value from arduino
    print('Reading from device 0x%02x'%(devAddr))
    val = i2c.readBytes(devAddr, 0x00, 6)
    addr = val[0]
    value = array.array('h',val[1:])
    print('Read returned: ', val)
    print('addr:',addr)
    print('value:',value)
    cntr = cntr + 1
    time.sleep(5)
    
print('Done!')

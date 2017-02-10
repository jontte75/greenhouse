#i2ctest

This directory contains different i2c related test scripts, which will and have been used to prototype interface bbetween Omega2 and Arduino (Mega 2560).

##i2ctest.ino
This is the arduino code for testing i2c. It will communicate with ***i2c.py*** running on Omega2. No extra pullups used, relies on builtins in Arduino. Bi-directional Logic level  shifter is required because Arduino works in 5v and Omega2 in 3.3v.
<pre>
Arduino                              LLS                           Omega2
*******                              ***                              ******
                                     _____
SCL<-------------------------------->|   |<------------------------->SCL
SDA<-------------------------------->|   |<------------------------->SDA
                                     -----           
</pre>
Arduino will work in slave mode, so that Omega2 will ask for data or toggle a led (nr 13)
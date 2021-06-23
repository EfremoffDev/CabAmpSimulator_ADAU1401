#functions for convert data to ADAU bytes.

def to_bytes_float(address, value):  #convert float value to ADAU-bytes, and create a correct list for send to ADAU via i2c bus in 5.23 format.
    hexArray = []
    shamanConstant = 8388608
    bigValue = float(value)*shamanConstant
    hexValue = hex(int(bigValue))
    if float(value) < 0:
        val = int(hexValue,16)
        if (val & (1 << (32 - 1))) != 0:
            val = val - (1 << 32)
        hexValue = str(hex(val & ((2 ** 32) - 1)))
    while len(hexValue) < 8:
        hexValue = '0' + hexValue
    if float(value) >= 0:
        print('Positive Value Coming')
        hexValue = '00' + str(hexValue).replace('0x', '')
    else:
        print('negative Value Coming')
        hexValue = str(hexValue).replace('0x', '')

    for i in range(0, 8, 2):
        hexString = '0x' + hexValue[i: i+2]
        an_integer = int(hexString, 16)
        hexArray.append(an_integer)

    hexBytesArray = ["0x%02X" % b for b in hexArray]
    #print ([address>>8, address&0xFF, int(hexBytesArray[0],16),int(hexBytesArray[1],16), int(hexBytesArray[2],16), int(hexBytesArray[3],16)])
    return [address>>8, address&0xFF, int(hexBytesArray[0],16),int(hexBytesArray[1],16), int(hexBytesArray[2],16), int(hexBytesArray[3],16)]

def to_bytes_int(address, value):  #convert int value to ADAU-bytes, and create a correct list for send to ADAU via i2c bus in 28.0 format.
    x4 = value & 0xFF
    value = value >> 8
    x3 = value & 0xFF
    value = value >> 8
    x2 = value & 0xFF
    value = value >> 8
    x1 = value & 0xFF
    #print("0x%02X" % x1, "0x%02X" % x2, "0x%02X" % x3, "0x%02X" % x4)
    return [address>>8, address&0xFF, x1, x2, x3, x4]
#import nesessery microPython libs
import machine
import utime
import sdcard
import os
from rotary_irq_rp2 import RotaryIRQ  #lib for rotary encoder

#SetUp Started

sda=machine.Pin(4)  #specified i2c_0 SDA pin
scl=machine.Pin(5)  #specified i2c_0 SCL pin

cabSwitch = machine.Pin(8, machine.Pin.IN) #set cabinet encoder switch button.
cabLED = machine.Pin(14, machine.Pin.OUT) #set cabinet status LED output.
cabSwitchFlag = 0 #special var for anti-bounce for cabinet switch.

ampSwitch = machine.Pin(22, machine.Pin.IN) #set amplifier encoder switch button.
ampLED = machine.Pin(26, machine.Pin.OUT) #set amplifier status LED output.
ampSwitchFlag = 0 #special var for anti-bounce for cabinet switch.

#set i2c bus and scan available adresses

i2c=machine.I2C(0,sda=sda, scl=scl, freq=400000) 
 
print('Scan i2c bus...')
devices = i2c.scan()
 
if len(devices) == 0:
  print("No i2c device !")
else:
  print('i2c devices found:',len(devices))
 
  for device in devices:  
    print("Decimal address: ",device," | Hexa address: ",hex(device))
   
#Function find a i2c devices. 0x34 is a ADAU1401 DSP.
#Get last saved volume data from local .txt files

try:
    with open('log_amp_vol.txt', 'r') as data:
        currentDataAmpVol = data.read()
        #print("AmpVol Values: " + currentDataAmpVol)
except:
    currentDataAmpVol = 10
    pass
try:
    with open('log_cab_vol.txt', 'r') as data:
        currentDataCabVol = data.read()
        #print("CabVol Values: " + currentDataCabVol)
except:
    currentDataCabVol = 0.5
    pass

#init encoders for Amplifier Volume and Cabinet Volume

encoder_one = RotaryIRQ(
            pin_num_clk=20, 
            pin_num_dt=21, 
            min_val=0, 
            max_val=20, 
            reverse=True, 
            range_mode=RotaryIRQ.RANGE_BOUNDED)

encoder_two = RotaryIRQ(
            pin_num_clk=6, 
            pin_num_dt=7, 
            min_val=0, 
            max_val=20, 
            reverse=True, 
            range_mode=RotaryIRQ.RANGE_BOUNDED)

encoder_one.set(int(currentDataAmpVol)) #set start position for Amplifier encoder from last local saved state
encoder_two.set(int(currentDataCabVol)) #set start position for Cabinet encoder from last local saved state

val_old_one = encoder_one.value()
val_old_two = encoder_two.value()
startedTime = int(utime.time()) + 3 #set time for record volume changes into local files - 3s. 

utime.sleep_ms(100) #pre-start delay 100ms.

ampOn = 0x0046 # amplifier mono switch register from SigmaStudio capture window. Can be 0 (off) or 1 (on). Route signal from direct to Amplifier simulator stage. 
ampVol = 0x003C # amplifier single volume slider (ampVol in SigmaStudio project) register from SigmaStudio capture window. Float value available.
cabOn = 0x0318 # cabinet mono switch register from SigmaStudio capture window. Can be 0 (off) or 1 (on). Route signal from direct to FIR filter stage. 
cabVol = 0x0047 # cabinet single volume slider (cabVol in SigmaStudio project) register from SigmaStudio capture window. Float value available.
stateCab = 0 #start off state for cabinet simulation
stateAmp = 0  #start off state for cabinet simulation

#write off state to ADAU1401 adress for selected register TODO: 0x34 need to be changed on variable.
i2c.writeto(0x34, bytes(to_bytes_int(cabOn, stateCab)))
i2c.writeto(0x34, bytes(to_bytes_int(ampOn, stateAmp)))  

#write last-save state to ADAU1401 adress for selected register 
i2c.writeto(0x34, bytes(to_bytes_float(ampVol, int(currentDataAmpVol)/10.1)))
i2c.writeto(0x34, bytes(to_bytes_float(cabVol, int(currentDataCabVol)/105)))

#SetUp Finished 

#Main Loop Started

while True:
    currentTime = int(utime.time())
    #check encoder state changes
    val_new_one = encoder_one.value()
    val_new_two = encoder_two.value()
    
    if val_old_one != val_new_one:
        val_old_one = val_new_one
        print('result Amp Vol =', val_new_one)
        i2c.writeto(0x34, bytes(to_bytes_float(ampVol, val_new_one/10.1))) #write float value of amplifier volume to ADAU1401 adress for selected register 
        startedTime = currentTime
        
    if val_old_two != val_new_two:
        val_old_two = val_new_two
        print('result Cab volume =', val_new_two)
        i2c.writeto(0x34, bytes(to_bytes_float(cabVol, val_new_two/105)))  #write float value of cabinet volume to ADAU1401 adress for selected register 
        startedTime = currentTime
    
    #check buttons state for on/off cabinet and amplifier. 
    
    if not cabSwitch.value() and cabSwitchFlag == 0:
        if stateCab == 0:
            cabLED.value(1)
            i2c.writeto(0x34, bytes(to_bytes_int(cabOn, 1))) #write int value of cabinet on to ADAU1401 adress for selected register 
            stateCab = 1
        else:
            cabLED.value(0)
            i2c.writeto(0x34, bytes(to_bytes_int(cabOn, 0))) #write int value of cabinet off to ADAU1401 adress for selected register 
            stateCab = 0
        cabSwitchFlag = 1
    if cabSwitch.value() and cabSwitchFlag == 1:
        cabSwitchFlag = 0
    
    if not ampSwitch.value() and ampSwitchFlag == 0:
        if stateAmp == 0:
            ampLED.value(1)
            i2c.writeto(0x34, bytes(to_bytes_int(ampOn, 1))) #write int value of amplifier on to ADAU1401 adress for selected register 
            stateAmp = 1
        else:
            ampLED.value(0)
            i2c.writeto(0x34, bytes(to_bytes_int(ampOn, 0))) #write int value of amplifier on to ADAU1401 adress for selected register 
            stateAmp = 0
        ampSwitchFlag = 1
    if ampSwitch.value() and ampSwitchFlag == 1:
        ampSwitchFlag = 0
        
    #save actual state of amplifier and cab volumes it user not rotate encoder longer than 3s. 
        
    if startedTime and currentTime > startedTime +3:
        with open('log_amp_vol.txt', 'w') as fileData:
            fileData.write('%d\n' % (val_new_one))
        with open('log_cab_vol.txt', 'w') as fileData:
            fileData.write('%d\n' % (val_new_two))
        startedTime = 0
    utime.sleep_ms(5)




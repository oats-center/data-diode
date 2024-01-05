import serial
import io
import time
import os
 
signal = serial.Serial(
    port='COM7',
    baudrate=115200,
    bytesize=8,
    parity='N',
    timeout=1,
    stopbits=1
)
 
signal_text = io.TextIOWrapper(signal)
file_name = "com_data_stm.txt"
 
aaa = ""      
while True:
    aaa = signal_text.readline()
    res = aaa.translate(str.maketrans('','',"|"))
    res = res.translate(str.maketrans('','',"_"))
    if(res):
        print(res)
        with open(file_name, "a") as output:   #open file in append mode
                    output.write(res)
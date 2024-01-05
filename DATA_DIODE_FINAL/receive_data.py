import socket
import signal
import sys

def signal_handler(sig, frame):
    print('Pressed Ctrl+C!')
    print("Exiting...")
    sys.exit(0)

UDP_PORT = 6969
interface = "128.46.213.185" #Public IP Address of the system
file_name = "received_data.txt"

sock = socket.socket(socket.AF_INET, # Ethernet
                     socket.SOCK_DGRAM) # UDP
sock.bind((interface, UDP_PORT))
signal.signal(signal.SIGINT, signal_handler)
print('Press Ctrl+C to exit')

while True:
    try:
        data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes    
        print("received message: %s" %  data)
        if (len(data) == 506):
            with open(file_name, "a") as output:   #open file in append mode
                output.write(data.decode("utf-8") )
                output.write('\n')
    except Exception as e:
        print("i'm here")
        print(e)
        pass






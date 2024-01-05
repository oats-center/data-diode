from scapy.all import *
from scapy.utils import rdpcap
from scapy.utils import PcapWriter
import time
#pktdump = PcapWriter("C:/Users/kmani/OneDrive/Desktop/Data Diode/PCAP_CAPTURES/to_send2.pcap", append=True, sync=True)

 
pkts=rdpcap("pcap_files/east_intersection.pcap",1)   # rdpcap("filename",500) fetches first 500 pkts
#pkts=rdpcap("pcap_files/econolite_241.pcap",1) 
#pkts=rdpcap("pcap_files/allTraffic.pcap",10)
#pkts=rdpcap("pcap_files/west_intersection.pcap",200)

def b2i(byte):
    '''
    Convert bytes to an integer.
    '''
    return int.from_bytes(byte, byteorder='big')

for i in range(1,1000):
    for pkt in pkts:
         try:
            pkt_in_byte = bytearray(bytes(pkt))
            pkt_in_byte = pkt_in_byte[42:]
            local_time = round(time.time() * 1000) #get time since epoch
            local_time_in_bytes = local_time.to_bytes(7,'big') #need 6 bytes to store the value 
            pkt_in_byte[230:237] = local_time_in_bytes #using bytes[230:237] for time; unrelated to the actual fields in the spat msg
            send(IP(dst="170.254.168.234")/UDP(dport=6053,sport=pkt[UDP].dport)/pkt_in_byte)
            time.sleep(0.1);
         except:
            pass
  
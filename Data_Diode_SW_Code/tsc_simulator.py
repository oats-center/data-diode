from scapy.all import *
from scapy.utils import rdpcap
from scapy.utils import PcapWriter
import time
#pktdump = PcapWriter("C:/Users/kmani/OneDrive/Desktop/Data Diode/PCAP_CAPTURES/to_send2.pcap", append=True, sync=True)

 
#pkts=rdpcap("pcap_files/east_intersection.pcap",5)   # rdpcap("filename",500) fetches first 500 pkts
pkts=rdpcap("pcap_files/econolite_241.pcap",50) 
#pkts=rdpcap("pcap_files/allTraffic.pcap",10)
#pkts=rdpcap("pcap_files/west_intersection.pcap",200)


for pkt in pkts:
     try:
        pkt_in_byte = bytearray(bytes(pkt))
        pkt_in_byte = pkt_in_byte[42:]
        send(IP(dst="169.254.168.234")/UDP(dport=6053,sport=pkt[UDP].dport)/pkt_in_byte)
        time.sleep(0.1);
     except:
        pass
from scapy.all import *
from scapy.utils import rdpcap
from scapy.utils import PcapWriter
import time
#pktdump = PcapWriter("C:/Users/kmani/OneDrive/Desktop/Data Diode/PCAP_CAPTURES/to_send2.pcap", append=True, sync=True)


 
pkts=rdpcap("pcap_files/east_intersection.pcap",200)   # rdpcap("filename",500) fetches first 500 pkts
#pkts=rdpcap("pcap_files/allTraffic.pcap",1000)
#pkts=rdpcap("pcap_files/west_intersection.pcap",200)


for pkt in pkts:
     try:
        send(IP(dst="169.254.173.252")/UDP(dport=pkt[UDP].dport,sport=pkt[UDP].dport)/pkt)
        time.sleep(0.1);
     except:
        pass
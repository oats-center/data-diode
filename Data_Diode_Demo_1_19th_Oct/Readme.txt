1. The UDP PORT on STM_Tx file must match with that to which the "pcap_replay.py" binds
2. The IP address to bind to must be same as that in the STM_Tx file. This IP address inturn must be within the subnet that the laptop ethernet port exposes.
	i)   When the experiment was conducted, the ethernet IP address was 169.254.73.122 and the subnet mask was 255.255.0.0
	ii)  Thus, the binding must be done on any address in between 169.254.0.0 to 169.254.255.255 
	iii) In STM_Tx file the IP address chosen was 169.254.173.252 This must be the IP destination on "pcap_replay.py" as well i.e. IP(dst="169.254.173.252")
	iv)  Alternatively one could use 169.254.255.255 in "pcap_replay.py" and broadcast the pkt. In this case the IP used to connect the STM_Tx does not matter. However, even here the STM_Tx IP address must be within in the same subnet.
2. Provide proper file path of pcap file in "pcap_replay.py"
3. Look at HOW_TO_RUN.png to see method to run "pcap_replay.py" and the command window output observed after run
4. Adjust the time between the packets using time.sleep() function. The delay between the packets in the pcap file is ~ 100ms. So time.sleep(0.1) is input
5. "allTraffic.pcap" file has many protocol packets(tcp, http, etc). Only udp to port 6054 will be received on STM32_Tx
6. "west_intersection.pcap" file has destination port set as 6053. So same has to be used in STM_Tx file. (Alternatively the data can be binded to 6054 itself. For this just use UDP(dport=6054)
7. Demo pictures can be seen in Presentation_Demo.pdf
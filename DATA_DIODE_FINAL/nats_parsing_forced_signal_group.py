import asyncio
import time
import json
from nats.aio.client import Client as NATS
import psycopg2
from psycopg2 import Error
import base64
import binascii

#3831353334315104001C002A : PCB diode
#38313533343151160034003C : Polycase diode
USERNAME ="diode",
PASSWORD ="9c7TCRO"
STM_to_Intersection_ID = {"383135333431511600330032":"179", "3831353334315104001C002A":"12544", "38313533343151160034003C":"53186"}
cursor = 0
                           
'''
Example parsing TSCBM formatted method with Python.
'''

def b2i(byte):
    '''
    Convert bytes to an integer.
    '''
    return int.from_bytes(byte, byteorder='big')

def readB(byte, offset, size):
    '''
    Split an array of bytes, and return the next location after this is removed.
    '''
    return byte[offset:offset+size], offset+size

def hextobin(hexval):
    '''
    Takes a string representation of hex data with
    arbitrary length and converts to string representation
    of binary.  Includes padding 0s
    '''
    thelen = len(hexval)*4
    binval = bin(int(hexval, 16))[2:]
    while ((len(binval)) < thelen):
        binval = '0' + binval
    return binval

def parse_TSCBM(current_time, id, bytes):
    '''
    Takes apart a TSCBM formatted SPaT message and makes a JSON object
    '''
    #byte 0: DynObj13 response byte (0xcd)
    #This is always CD

    #byte 1: number of phase/overlap blocks below (16) 
    block = b2i(bytes[1:2]) #The number of blocks of phase/overlap below 16.
    offset = 2 #Start at offset 2 and loop until Range(blocks)
    phases=[]
    for i in range(block):
        #0x01 (phase#)	(1 byte) 
        outPhase, offset = readB(bytes, offset, 1) #2
        outVehMin, offset = readB(bytes, offset, 2) #3,4
        outVehMax, offset = readB(bytes, offset, 2) #5, 6
        outPedMin, offset = readB(bytes, offset, 2) #7, 8
        outPedMax, offset = readB(bytes, offset, 2) #9, 10
        outOvlpMin, offset = readB(bytes, offset, 2) #11, 12 Overlap min
        outOvlpMax, offset = readB(bytes, offset, 2) #13, 14 Overlap Max

        phase = {
            "phase": b2i(outPhase),
            "color": 'RED',
            "flash": False,
            "dont_walk": False,
            "walk": False,
            "pedestrianClear": False,
            "overlap": {
                "green": False,
                "red": False,
                "yellow": False,
                "flash": False
            },
            "vehTimeMin": round((b2i(outVehMin) or 0) * .10, 1), #self.__b2i(out_VMin), 
            "vehTimeMax": round((b2i(outVehMax) or 0) * .10, 1), #self.__b2i(out_VMax), 
            "pedTimeMin": round((b2i(outPedMin) or 0) * .10, 1), #self.__b2i(out_PMin), # round(self.__b2i(out_PMin) * Decimal(.10), 1),
            "pedTimeMax": round((b2i(outPedMax) or 0) * .10, 1), #self.__b2i(out_PMax), # round(self.__b2i(out_PMax) * Decimal(.10), 1),
            "overlapMin": round((b2i(outOvlpMin) or 0) * .10, 1),
            "overlapMax": round((b2i(outOvlpMax) or 0) * .10, 1)
        }
        phases.append(phase)

    # bytes 210-215: PhaseStatusReds, Yellows, Greens	(2 bytes bit-mapped for phases 1-16)
    PhaseStatusReds, offset = readB(bytes, offset, 2)
    PhaseStatusYellows, offset = readB(bytes, offset, 2)
    PhaseStatusGreens, offset = readB(bytes, offset, 2)

    # # bytes 216-221: PhaseStatusDontWalks, PhaseStatusPedClears, PhaseStatusWalks (2 bytes bit-mapped for phases 1-16)
    PhaseStatusDontWalks, offset = readB(bytes, 216, 2)
    PhaseStatusPedClears, offset = readB(bytes, 218, 2)
    PhaseStatusWalks, offset = readB(bytes, 220, 2)

    # bytes 222-227: OverlapStatusReds, OverlapStatusYellows, OverlapStatusGreens (2 bytes bit-mapped for overlaps 1-16)
    OverlapStatusReds, offset = readB(bytes, 222, 2)
    OverlapStatusYellows, offset = readB(bytes, 224, 2)
    OverlapStatusGreens, offset = readB(bytes, 226, 2)
    # bytes 228-229: FlashingOutputPhaseStatus	(2 bytes bit-mapped for phases 1-16)
    FlashingOutputPhaseStatus, offset = readB(bytes, 228, 2)

    # bytes 230-231: FlashingOutputOverlapStatus	(2 bytes bit-mapped for overlaps 1-16)
    FlashingOutputOverlapStatus, offset = readB(bytes, 230, 2)

    # byte 232: IntersectionStatus (1 byte) (bit-coded byte) 
    IntersectionStatus, offset = readB(bytes, 232, 1)
    
    # Byte 233: TimebaseAscActionStatus (1 byte)  	(current action plan)                       
    # byte 234: DiscontinuousChangeFlag (1 byte)          (upper 5 bits are msg version #2, 0b00010XXX)     
    # byte 235: MessageSequenceCounter (1 byte)           (lower byte of up-time deci-seconds) 
    
    # Byte 236-238: SystemSeconds (3 byte)	(sys-clock seconds in day 0-84600)      
    SystemSeconds, offset = readB(bytes, 236, 3)          
    
    # Byte 239-240: SystemMilliSeconds (2 byte)	(sys-clock milliseconds 0-999)  
    SystemMilliSeconds, offset = readB(bytes, 239, 2)
    
    # Byte 241-242: PedestrianDirectCallStatus (2 byte)	(bit-mapped phases 1-16)             
    # Byte 243-244: PedestrianLatchedCallStatus (2 byte)	(bit-mapped phases 1-16)  
    
    TSCtime = '{}.{}'.format(b2i(SystemSeconds), b2i(SystemMilliSeconds))
    #Set lights to Green/Yellow/Flash by phase.

    greens = hextobin(PhaseStatusGreens.hex())
    greens_overlap = hextobin(OverlapStatusYellows.hex())
    yellows = hextobin(PhaseStatusYellows.hex())
    yellows_overlap = hextobin(OverlapStatusYellows.hex())
    reds = hextobin(PhaseStatusReds.hex())
    reds_overlap = hextobin(OverlapStatusReds.hex())

    flashing = hextobin(FlashingOutputPhaseStatus.hex())
    flashing_overlap = hextobin(FlashingOutputOverlapStatus.hex())
    dont_walk = hextobin(PhaseStatusDontWalks.hex())
    walk = hextobin(PhaseStatusWalks.hex())
    pedClear = hextobin(PhaseStatusPedClears.hex())

    for phase in phases:
        index = phase['phase']
        if yellows[16-index] == '1':
            phase['color'] = "YELLOW"
        if greens[16-index] == '1':
            phase['color'] = "GREEN"
        if greens_overlap[16-index] == '1':
            phase['overlap']['green'] = True
        if yellows_overlap[16-index] == '1':
            phase['overlap']['yellow'] = True
        if reds_overlap[16-index] == '1':
            phase['overlap']['red'] = True
        if flashing[16-index] == '1':
            phase['flash'] = True
        if flashing[16-index] == '1':
            phase['overlap']['flash'] = True
        if dont_walk[16-index] == '1':
            phase['dont_walk'] = True
        if walk[16-index] == '1':
            phase['walk'] = True
        if pedClear[16-index] == '1':
            phase['pedestrianClear'] = True

    SenderTimeinBytes, _ = readB(bytes, 230, 7)
    SenderTime = b2i(SenderTimeinBytes)
    payload = {
        'id': id,
        'current_time': current_time,
        'TSCtime': TSCtime,
        'SenderTime': SenderTime,
        'phases': phases
    }

    return payload
    
async def receive():
    nc = NATS()

    async def disconnected_cb():
        print("Got disconnected...")

    async def reconnected_cb():
        print("Got reconnected...")

    await nc.connect("ibts-compute.ecn.purdue.edu:4223",
                     reconnected_cb=reconnected_cb,
                     disconnected_cb=disconnected_cb,
                     user ="diode",
                     password ="9c7TCRO",
                     max_reconnect_attempts=-1)
    
    async def help_request(msg):
        try:
            subject = msg.subject
            reply = msg.reply
            spat_data = msg.data.decode()
            stm_id = subject.replace("traffic.","")
            #print(stm_id)
            intersection_id = STM_to_Intersection_ID[stm_id]
            #print("Received a message on '{subject} {reply}': {data}".format(subject=intersection_id, reply=reply, data=spat_data))
            hex_result = base64.b64decode(spat_data).hex().upper()
            print("Received a message on {subject} : {spat_hex_result}".format(subject=intersection_id, spat_hex_result=hex_result[:-8]))
            crc32_little_endian = hex_result[-8:]
            crc32_bytes = bytearray.fromhex(str(crc32_little_endian))
            crc32_bytes.reverse()
            crc32_big_endian = ''.join(format(x, '02x') for x in crc32_bytes)
            base16INT = int(crc32_big_endian, 16)
            crc32_big_endian = hex(base16INT) #convert string to hex
            publish_to = "light-status." + intersection_id
            if(hex_result and (crc32_big_endian == hex(binascii.crc32(binascii.a2b_hex(hex_result[:-8]))))):
            #if(hex_result):
                payload = parse_TSCBM(round(time.time() * 1000), intersection_id, bytes.fromhex(hex_result[:-8]))
                payload_json = json.dumps(payload) 
                payload_json_bytes = bytes(payload_json, 'utf-8')
                #print(payload_json_bytes)
                await nc.publish(publish_to,payload_json_bytes)
        except Exception:
            pass

    async def nearest_parsing(msg):
        print(msg)
        curr_location  = msg.data
        curr_location = json.loads(curr_location.decode('utf-8'))
        cursor.execute('SELECT intersection_id, signal_group, maneuvers, ST_DistanceSphere(geo, ST_SetSRID(ST_MakePoint(' + str(curr_location["lat"]) + ',' + str(curr_location["lon"]) +'), 4326)) \
                        as dist FROM lanes ORDER BY dist ASC LIMIT 1')
        lane_record = cursor.fetchall() #Result  [(12544, 2, '{right}', 72.38811965)]
        #print(lane_record)
        lane_record = [(53186, 1, '{right,straight}', 72.38811965)] #forcing for testing purpose
        print(f"dist = {lane_record[0][3]}")
        return_record = {}
        return_record["intersectionId"] = lane_record[0][0]
        return_record["signalGroup"] = lane_record[0][1]
        return_record["serverTime"] = round(time.time() * 1000)
        laneManeuvers_string = lane_record[0][2]
        return_record["laneManeuvers"] = laneManeuvers_string.strip('}{').split(',')
        return_record = json.dumps(return_record)
        print(return_record)
        await nc.publish(msg.reply,bytes(str(return_record),'utf-8'))
    
    await nc.subscribe("traffic.>", cb=help_request)
    await nc.subscribe("light-nearest", cb=nearest_parsing)
    
    
if __name__ == '__main__':
    try:
        # Connect to postgres database
        connection = psycopg2.connect(user="postgres",
                                      password="ynj8buz",
                                      host="128.46.199.235",
                                      port="5432",
                                      database="postgres")
                                      
        cursor = connection.cursor()
        print("PostgreSQL server information")
        print(connection.get_dsn_parameters(), "\n")
        # Executing a SQL query
        cursor.execute("SELECT version();")
        # Fetch result
        record = cursor.fetchone()
        print("You are connected to - ", record, "\n")
    except (Exception, Error) as error:
        print("Error while connecting to PostgreSQL", error)
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(receive())
    try:
        loop.run_forever()
    finally:
        loop.close()
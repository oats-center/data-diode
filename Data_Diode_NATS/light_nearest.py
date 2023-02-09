import asyncio
import time
import json
from nats.aio.client import Client as NATS
import psycopg2
from psycopg2 import Error

STM_to_Intersection_ID = {"383135333431511600330032":"53693", "3831353334315104001C002A":"179"}
cursor = 0

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

    async def nearest_parsing(msg):
        print(msg)
        curr_location  = msg.data
        curr_location = json.loads(curr_location.decode('utf-8'))
        cursor.execute('SELECT intersection_id, signal_group, maneuvers, ST_DistanceSphere(geo, ST_SetSRID(ST_MakePoint(' + str(curr_location["lat"]) + ',' + str(curr_location["lon"]) +'), 4326)) \
                        as dist FROM lanes ORDER BY dist ASC LIMIT 1')
        lane_record = cursor.fetchall() #Result  [(12544, 2, '{right}', 72.38811965)]
        print(f"dist = {lane_record[0][3]}")
        return_record = {}
        return_record["intersectionId"] = lane_record[0][0]
        return_record["signalGroup"] = lane_record[0][1]
        laneManeuvers_string = lane_record[0][2]
        return_record["laneManeuvers"] = laneManeuvers_string.strip('}{').split(',')
        return_record = json.dumps(return_record)
        print(return_record)
        await nc.publish(msg.reply,bytes(str(return_record),'utf-8'))
    
    await nc.subscribe("light-nearest", cb=nearest_parsing)
    
    
if __name__ == '__main__':
    #loop = asyncio.get_event_loop()
    try:
        # Connect to an existing database
        connection = psycopg2.connect(user="postgres",
                                      password="ynj8buz",
                                      host="128.46.199.238",
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
import paho.mqtt.client as mqtt
import time
import json
from datetime import datetime as dt
from pymongo import MongoClient as mongoC

#define standard signal

measured_power = -58
N=2; # Environmental Factor - varies from 2 - 4 (Need to calibrate for different distances indoors)

#Define pymongo client

c=mongoC("mongodb://localhost:27017")
db = c['BLETracker']
collection = db.Beacon_Signals_Test
measured_dist={}
measured_dist['ESP3201']=1.5
measured_dist['ESP3202']=2.8
measured_dist['ESP3203']=2.6

### Set up MQTT Broker details
broker = "192.168.4.54"
port = 1883
user = "narada"
pwd = "narada$"
topic = "BLE_beacon_details"

### Define client connection event funcs
def on_connect(client, userdata, flags, message):
    client.subscribe(topic,0)

def on_message(client, userdata, message):
    d_ = json.loads(message.payload.decode('utf-8'))
    if d_['beacon']=='T87B2_CDD6':
        d_['time']=dt.now()
        diff_power = measured_power-d_['RSSI']
        denom = 10*N

        d_['distance'] = pow(10,diff_power/denom) #simple measure - needs better work; Need error margin calculation
        d_['error']=d_['distance']-measured_dist[d_['sender']]
        id=collection.insert_one(d_).inserted_id
        print(id)

### define MQTT client and connect to broker

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,"test")
client.username_pw_set(user,pwd)
client.connect(broker,port)

### Call event functions to get published messages

client.on_connect=on_connect
client.on_message=on_message
client.loop_forever()




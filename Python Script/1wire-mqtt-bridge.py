#!/usr/bin/env python3
"""
Python script to monitor serial port and forward sensor data to mqtt broker
"""

from __future__ import absolute_import, division, print_function, \
                                                    unicode_literals
import serial
import json
import datetime
import sys  
import re
import paho.mqtt.client as mqtt

DEBUG = True


def trace(output):
    if DEBUG:
        print(output)


mqtt_topics = {
    "28-68-4d-c4-0b-00-00-8f": "/home/bathroom/temperature", #BATHROOMSENSOR
    "28-8c-37-c4-03-00-00-8f": "/home/water/cylinderbase", #CYLINDER_BASE
    "28-89-45-c4-03-00-00-8a": "/home/water/cylindertop", #CYLINDER_TOP
    "28-46-81-c4-0b-00-00-29": "/home/water/cylinderoutlet", #CYLINDER_OUTLET
    "3a-e1-54-63-00-00-00-13": "/home/heating", # Nest heating controller
}

mqtt_broker = '192.168.1.20'
mqtt_user = 'username'
mqtt_pass = 'password'
mqtt_port = 1883

# The callback for when the client receives a CONNACK response from the server.
def on_connect(mqttc, obj, flags, rc):
    if rc!=0:
       trace("Bad connection Returned code=",rc)
       # trace("MQTT Connected with result code "+str(rc))
    #else:
        

def on_disconnect(client, userdata, rc):
    trace("MQTT Disconnected with result code "+str(rc))

def main():

    client = mqtt.Client("P1") #create new instance
    client.username_pw_set(mqtt_user, mqtt_pass)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    

    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=20)  # open serial port
    ser.flush()
   #client.loop_forever()

    while True:
        ser.read(1)

        if ser.in_waiting > 0:
            line = ser.readline()
            str1 = "{"
            str2 = ''.join(map(chr, line))
            data = str1 + str2

            ser.flush()
            try:
                now = datetime.datetime.now()
                #trace(str(now))
                #trace(line)
                jsonObj = json.loads(data)

                temperature_sensors = jsonObj["temperatures"]

                client.connect(mqtt_broker, mqtt_port, 60)

                for tsensor in temperature_sensors:
                    #trace("Topic: " + mqtt_topics[tsensor["address"]])
                    #trace("Value: " + tsensor["value"])
                    if float(tsensor["value"]) > -10 and float(tsensor["value"]) < 85:
                        client.publish(mqtt_topics[tsensor["address"]],tsensor["value"], retain=True)

                switch_sensors = jsonObj["switches"]
                
                for ssensor in switch_sensors:
                    #trace("Topic: " + mqtt_topics[ssensor["address"]])
                    #trace("solarpump: " + ssensor["pioa"])
                    #trace("solarcontrollerpower: " + ssensor["piob"])
                    client.publish(mqtt_topics[ssensor["address"]] + "/hotwater",ssensor["pioa"])
                    client.publish(mqtt_topics[ssensor["address"]] + "/centralheating",ssensor["piob"])
                client.disconnect()
            except Exception as err:
                trace("Failed to parse json: {0}".format(err))
if __name__ == "__main__":
    main()

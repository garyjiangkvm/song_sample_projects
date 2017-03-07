#!/bin/bash

#list
curl -H "Content-Type: application/json" -X GET -d '{"DriverName":"COMET"}' http://192.168.14.200:9876/volume/list

#Create
curl -H "Content-Type: application/json" -X POST -d '{"VolumeID":"xyz","DriverName":"COMET","Capacity":"100"}' http://192.168.14.200:9876/volume/create

#inspect
curl -H "Content-Type: application/json" -X GET -d '{"VolumeID":"xyz","DriverName":"COMET"}' http://192.168.14.200:9876/volume

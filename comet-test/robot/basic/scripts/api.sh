#!/bin/bash

#api.sh HOST_PORT ACTION VOLUME CAPACITY

DRV=COMET
REPLICATION=r1
HOST_PORT=$1

#echo $# $1 $2 $3 $4

#list
if [[ "$#" = 2 && $2 = "list" ]]; then
    JSON="{\"DriverName\":\"$DRV\"}"
    curl -s -H "Content-Type: application/json" -X POST -d $JSON http://${HOST_PORT}/volume/list
fi

#create
if [[ "$#" = 4 && $2 = "create" ]]; then
    JSON="{\"VolumeID\":\"$3\",\"DriverName\":\"$DRV\",\"Capacity\":\"$4\"}"
    echo $JSON
    curl -s -H "Content-Type: application/json" -X POST -d $JSON http://${HOST_PORT}/volume/create
fi

#inspect
if [[ "$#" = 3 && $2 = "inspect" ]]; then
    JSON="{\"VolumeID\":\"$3\",\"DriverName\":\"$DRV\"}"
    curl -s -H "Content-Type: application/json" -X POST -d $JSON http://${HOST_PORT}/volume/inspect
fi

#delete
if [[ "$#" = 3 && $2 = "delete" ]]; then
    JSON="{\"VolumeID\":\"$3\",\"DriverName\":\"$DRV\"}"
    curl -s -H "Content-Type: application/json" -X POST -d $JSON http://${HOST_PORT}/volume/delete
fi

#!/bin/bash

THIS_PATH="${BASH_SOURCE[0]}";
THIS_DIR=$(dirname $THIS_PATH)
VOL_DRV=codrv
IMAGE=song/ubuntu-curl:latest

if [ ! "$#" = 3  -a ! "$#" = 4 ]; then
    echo $1-Arg-Robot-Fail
    exit
fi

TEST_CASE=$1
VOL_NAME=${TEST_CASE}-${2}
DIR_NAME=/data${2}
FILE_NAME=$DIR_NAME/$VOL_NAME
HOST_PORT=$3
#HOST=${HOST_PORT%:*} we can get HOST from HOST_PORT but try to use argument to pass to writetime
HOST=$4  #etcd host to write time stamp key

DIR_SCRIPTS=/root/dev/comet-test/robot/basic/scripts
TIME_LOOP=600

#run docker
docker run -d --name $VOL_NAME -v $DIR_SCRIPTS/writetime.sh:/root/writetime.sh -v $VOL_NAME:$DIR_NAME --volume-driver=$VOL_DRV $IMAGE bash /root/writetime.sh $FILE_NAME $TIME_LOOP $HOST


inspect_volume_mount ()
{
    #check docker volume
    VAR=`docker volume inspect "$1"`
    RESULT=`expr match "$VAR" ".*COMET/mounts"`   #if mounts exist, it means volume has been formatted and mount into container

    echo $VAR $RESULT

    if [ $RESULT -ne 0 ]; then
        echo Volume-Mount-Good
    else
        echo Volume-Mount-Robot-Fail
    fi
    
}

inspect_volume_cap ()
{
    CAP=`lsblk -l | grep /mounts/$1 | awk '{print $4}'`
    if [ -z $CAP ]; then #CAP is zero length
        echo $1-CAP-Robot-Fail
    else
        echo $1-CAP-GOOD-$CAP
    fi
        
}

inspect_api ()
{
    VAR=`bash $THIS_DIR/api.sh $1 inspect $2`
    RESULT=`expr match "$VAR" ".*$2"` 

    echo $VAR $RESULT

    if [ $RESULT -ne 0 ]; then
        echo API-Check-Good
    else
        echo API-Check-Robot-Fail
    fi
    
}

echo host_port=$HOST_PORT

inspect_volume_mount $VOL_NAME
inspect_volume_cap $VOL_NAME
inspect_api $HOST_PORT $VOL_NAME

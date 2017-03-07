#!/bin/bash

#$1 is how many volumes we would like to create

THIS_PATH="${BASH_SOURCE[0]}";
THIS_DIR=$(dirname $THIS_PATH)
VOL_DRV=codrv
IMAGE=song/ubuntu-curl:latest

if [ ! "$#" = 3 ]; then
    echo $1-Arg-Robot-Fail
    exit
fi

TEST_CASE=$1-$2
HOST_PORT=$3

DIR_SCRIPTS=/root/dev/comet-test/robot/basic/scripts

#get volumes ready
MAX=5
ARG_VOL=""
ARG_CMD=" bash /root/writetime4.sh $TEST_CASE $MAX "

for (( i = 0; i < ${MAX}; i++));
do
    VOL_NAME=${TEST_CASE}-${i}
    DIR_NAME=/data${i}
    FILE_NAME=$DIR_NAME/$VOL_NAME
    ARG_VOL="${ARG_VOL} -v ${VOL_NAME}:${DIR_NAME} --volume-driver=$VOL_DRV"
done;

#echo $ARG_VOL
#echo $ARG_CMD

#run docker
docker run -d --name $TEST_CASE -v $DIR_SCRIPTS/writetime.sh:/root/writetime.sh -v $DIR_SCRIPTS/writetime4.sh:/root/writetime4.sh $ARG_VOL $IMAGE $ARG_CMD

sleep 3s

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

#check docker volume
for (( i = 0; i < ${MAX}; i++));
do
    VOL_NAME=${TEST_CASE}-${i}
    inspect_volume_mount $VOL_NAME
    inspect_volume_cap $VOL_NAME
    inspect_api $HOST_PORT $VOL_NAME
done;

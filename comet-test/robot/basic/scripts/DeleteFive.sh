#!/bin/bash

#This test is related with CreateOne.sh

THIS_PATH="${BASH_SOURCE[0]}";
THIS_DIR=$(dirname $THIS_PATH)
VOL_DRV=codrv

if [ ! "$#" = 3 ]; then
    echo $1-Arg-Robot-Fail
    exit
fi

TEST_CASE=$1-${2}
DIR_NAME=/data${2}
FILE_NAME=$DIR_NAME/$VOL_NAME
HOST_PORT=$3
MAX=5

#docker volume rm
volume_in_use ()
{
    VAR=`docker volume rm "$1" 2>&1`
    RESULT=`expr match "$VAR" ".*volume is in use"`

    echo var=$VAR result=$RESULT

    if [ $RESULT -ne 0 ]; then       
        echo Volume-In-Use-Good
    else
        echo Volume-In-Use-Robot-Fail
    fi
}
for (( i = 0; i < ${MAX}; i++));
do
    VOL_NAME=${TEST_CASE}-${i}
    volume_in_use $VOL_NAME
done;


#remove docker container
docker stop $TEST_CASE
docker rm $TEST_CASE


sleep 3s


volume_mount ()
{
    #check docker volume
    VAR=`docker volume inspect "$1"`
    RESULT=`expr match "$VAR" ".*$1"`   #if volume exist, it means volume has been formatted and mount into container

    echo var=$VAR result=$RESULT

    if [ $RESULT -ne 0 ]; then       #just opposite, if volume is still there, test failed.
        echo Volume-Mount-Robot-Fail
    else
        echo Volume-Mount-Good
    fi
    
}
for (( i = 0; i < ${MAX}; i++));
do
    VOL_NAME=${TEST_CASE}-${i}
    volume_mount $VOL_NAME
done;



#volume still exit with cosrv
volume_api ()
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
for (( i = 0; i < ${MAX}; i++));
do
    VOL_NAME=${TEST_CASE}-${i}
    volume_api $HOST_PORT $VOL_NAME
done;


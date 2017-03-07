#!/bin/bash
#Write a text into a file with a timely fashion

FILE_NAME=$1   #file to write to 
TIME_LOOP=$2   #how many time slots to write
HOST=$3

KEY=${FILE_NAME##*/}

if [ -z $HOST ]; then
    Start="0"
else
    Start=`curl -s http://$HOST:2379/v2/keys/$KEY | python -c "import json,sys;obj=json.load(sys.stdin);print obj['node']['value'];"`
    if ! [[ $Start == +([0-9]) ]]; then
        Start="0"
    else
        Start=`expr $Start + 1`
    fi
fi
    

echo "write time to file < $FILE_NAME > from $Start API host <$HOST>" >> ${FILE_NAME}

touch ${FILE_NAME}
for (( i = 0; i < ${TIME_LOOP}; i++));
do
    STAMP=`expr $i + $Start`
    TIME=`date "+%Y-%m-%d %H:%M:%S"`
    echo ${TIME} ${FILE_NAME}--order--$STAMP >> ${FILE_NAME}
    echo ${TIME} ${FILE_NAME}--order--$STAMP

    if ! [ -z $HOST ]; then
        curl -s http://$HOST:2379/v2/keys/$KEY -XPUT -d value="$STAMP" 1 > /dev/null 2>&1
    fi
    
    sleep 1s;
done;

echo "write time to file < $FILE_NAME > from $Start API host <$HOST> done!" >> ${FILE_NAME}

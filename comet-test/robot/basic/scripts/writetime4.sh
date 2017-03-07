#!/bin/bash
#Write a text into a file with a timely fashion

TEST_CASE=$1
MAX=$2

THIS_PATH="${BASH_SOURCE[0]}";
THIS_DIR=$(dirname $THIS_PATH)

echo "write time to file < $TEST_CASE > $MAX times"

for (( i = 0; i < ${MAX}; i++));
do {
    VOL_NAME=${TEST_CASE}-${i}
    DIR_NAME=/data${i}
    FILE_NAME=$DIR_NAME/$VOL_NAME

    bash $THIS_DIR/writetime.sh $FILE_NAME 60
    sleep 1s

} & done

wait

echo "write time to file < $TEST_CASE > $MAX times Done!"

#!/bin/bash

echo "--- Starting robot frame work container to do the tests ---"
docker run --name robot -v /root/dev:/root/dev stratouklos/robotframework python /root/dev/comet-test/robot/basic/basic-test.py $*
echo "--- Cleanup  container ---"
docker rm robot 1>/dev/null 2>&1

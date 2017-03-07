#!/bin/bash

curl -i -s -H "Content-Type: application/json" -X GET http://127.0.0.1:9183/func/replication/config?namespace=$1

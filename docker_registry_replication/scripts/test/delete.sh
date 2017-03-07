#!/bin/bash

curl -i -s -H "Content-Type: application/json" -X POST -d '{"namespace": "song", "target":"192.168.14.167", "action": "delete"}' http://127.0.0.1:5000/func/replication/job

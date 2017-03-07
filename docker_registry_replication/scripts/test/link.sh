#!/bin/bash

curl -i -s -H "Content-Type: application/json" -X POST -d '{"namespace": "song", "source":"192.168.14.166", "target":"192.168.14.167", "user_id": "replication_master", "password":"cm0kNTIzMDY=$", "immediate_start":1}' http://127.0.0.1:9183/func/replication/testlink

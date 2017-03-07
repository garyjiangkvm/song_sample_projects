#!/bin/bash

curl -i -s -H "Content-Type: application/json" -X POST -d '{"namespace": "song", "source":"127.0.0.1", "target":"192.168.14.167", "user_id": "replication_master", "password":"rm$52306", "immediate_start":0}' http://127.0.0.1:5000/func/replication/config

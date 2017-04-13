#!/bin/bash

echo -e "GET / HTTP/1.0\r\n" | sudo netcat -U /var/run/names/joke-service.sock
echo
echo -e "GET /stat HTTP/1.0\r\n" | sudo netcat -U /var/run/names/joke-service.sock

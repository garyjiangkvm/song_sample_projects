#!/usr/bin/python

import os
import threading
from subprocess import call
import fnmatch
import sys
import time
import random
import getopt

VERSION="1.0"

MOUNT_PATH="/var/lib/codrv/COMET/mounts/"

def is_sublist(a, b):
    if a == []: return True
    if b == []: return False
    return b[:len(a)] == a or is_sublist(a, b[1:])

def check_stamp(vol_name, time_stamp):
    #check result
    stamps=[]
    with open(MOUNT_PATH+vol_name+"/"+vol_name) as vfile:
        for line in vfile:
            #print line
            if '--order--' in line:
                stamp = line.split("--order--")[1]
                stamps.append(int(stamp.strip()))
    print "Read from stamp file:",stamps
    
    a = stamps
    b = range(int(time_stamp))
    if is_sublist(a,b):
        print "Check-Stamp-Good"
    else:
        print "Check-Stamp-Robot-Fail"
        
    return stamps


def setup_env():
    vol_name= sys.argv[1]
    time_stamp = sys.argv[2]
    
    return vol_name,time_stamp

def main():
    #setup test env
    vol_name, time_stamp = setup_env()
    print "Volume time stamp check. vol: %s MaxTimeStamp: %s " % (vol_name,time_stamp)

    try:
        check_stamp(vol_name, time_stamp)
    except:
        print "Unexpected error:", sys.exc_info()[0] 
        print "Check-Stamp-Except-Robot-Fail"

if __name__ == "__main__":
    sys.exit(main())

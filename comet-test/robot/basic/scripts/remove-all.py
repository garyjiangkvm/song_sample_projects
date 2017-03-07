#!/usr/bin/python

import os
import fnmatch
import sys

VERSION="1.0"

def main():
    retvalue = os.popen("bash api.sh 192.168.14.200:9876 list | grep Create").readlines()
    for s in retvalue:
        vol = s.split('\"')[1].split('/')[4]
        print vol
        retvalue = os.popen("bash api.sh 192.168.14.200:9876 delete %s" % vol).readlines()

if __name__ == "__main__":
    sys.exit(main())

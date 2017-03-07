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

TEST_PATH='/root/dev/comet-test/robot/basic/'
CREATE_PATH=""
DELETE_PATH=TEST_PATH+"delete"
OUTPUT_PATH="/root/dev/comet-test/output"

API_HOSTIP="192.168.14.200"
PORT="9876"
PASSWORD="123456"
ORDER="sort"
INSTALL="YES"
ALL_IP=""

THREAD_NUMBER="1"

config1={"api_host":API_HOSTIP, "port":PORT, "hostname":"red", "hostip":"192.168.14.40", "password":PASSWORD}
config2={"api_host":API_HOSTIP, "port":PORT, "hostname":"yellow", "hostip":"192.168.14.41", "password":PASSWORD}
config3={"api_host":API_HOSTIP, "port":PORT, "hostname":"green", "hostip":"192.168.14.50", "password":PASSWORD}
configs=[config1,config2,config3]

def all_ip_string():
    s=""
    for i,config in enumerate(configs):
        s = s+config['hostip'] + "+"
    return s[:-1]

class Test:
    def __init__(self, thread, config):
        self.api_hostip = config['api_host']
        self.port = config['port']
        self.thread = thread
        self.hostname = config['hostname'] 
        self.hostip = config['hostip']
        self.password = config['password']
        self.order = ORDER
        self.all_ip = all_ip_string()

        self.create_files = [] 
        for root, dir, files in os.walk(CREATE_PATH):
            for item in fnmatch.filter(files, "*.robot"):
                self.create_files.append(item)

        if self.order == "random":
            random.shuffle(self.create_files)
        elif self.order == "sort":
            print self.create_files
            self.create_files = sorted(self.create_files, key=lambda name : int(name.split(".")[0].split("-")[2]))

        self.delete_files = [] 
        for root, dir, files in os.walk(TEST_PATH+"delete"):
            for item in fnmatch.filter(files, "*.robot"):
                self.delete_files.append(item)
        if INSTALL == "YES":
            self.common_setup()
        
    def common_setup(self):
        test_case = self.hostname+"-"+"B-Common-setup"
        output_file = test_case 
        robot_file = TEST_PATH+"common/B-Common-setup.robot"
        cmd = "pybot -d %s -o %s -v TESTCASE:%s -v ALL:%s -v HOST:%s -v PASSWORD:%s -v APIHOST:%s -v PORT:%s %s" % ( OUTPUT_PATH, output_file, test_case, self.all_ip, self.hostip, self.password, self.api_hostip, self.port, robot_file )
        print "run test -->", cmd
        os.system(cmd)
        return

    def run_one_test(self, output_file, test_case, robot_file):
        cmd = "pybot -d %s -o %s -v TESTCASE:%s -v ALL:%s -v HOST:%s -v PASSWORD:%s -v APIHOST:%s -v PORT:%s %s" % ( OUTPUT_PATH, output_file, test_case, self.all_ip, self.hostip, self.password, self.api_hostip, self.port, robot_file )
        print "run test -->", cmd
        os.system(cmd)

        ispass = False
        #check result
        with open(OUTPUT_PATH+"/"+output_file+".xml", 'r') as searchfile:
            for line in searchfile:
                if 'All Tests' in line:
                    ispass = 'fail=\"0\"' in line

        return ispass

    def run_tests(self):
        for create_file in self.create_files:
            test_case = self.hostname+self.thread+"-"+create_file.split(".")[0]
            ispass = self.run_one_test(test_case, test_case, CREATE_PATH+"/"+create_file)
            if not ispass:
                print "\n --- Warning: run test ",test_case, "failed!! ---"
                return
            delete_file = create_file.replace("Create","Delete")
            if delete_file in self.delete_files:
                ispass = self.run_one_test(test_case.replace("Create","Delete"),test_case, DELETE_PATH+"/"+delete_file)
                if not ispass:
                    print "\n --- Warning: run test ",test_case, "failed!! ---"
                    return
        return

class TestThread (threading.Thread):
    def __init__(self, threadID, name, config):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.name = name+threadID
        self.test = Test(threadID, config)

    def run(self):
        print "Starting " , self.name , time.ctime(time.time())
        self.test.run_tests()
        print "Exiting " + self.name, time.ctime(time.time())

def generate_report():
    cmd = "rebot --outputdir %s --output final_output.xml" % OUTPUT_PATH
    for root, dir, files in os.walk(OUTPUT_PATH):
        for item in fnmatch.filter(files, "*.xml"):
            cmd = cmd + " " + os.path.join(root, item)
    print "generate final report -->", cmd
    os.system(cmd)
    print
    print "--- Test Result ---"
    with open(OUTPUT_PATH+"/"+"final_output.xml", 'r') as searchfile:
        for line in searchfile:
            if 'All Tests' in line or 'Critical Tests' in line:
                print line

def setup_env():
    global CREATE_PATH,ORDER,INSTALL,THREAD_NUMBER
    os.system("rm -rf "+OUTPUT_PATH+"/*") #clear output files
    
    opts, args = getopt.getopt(sys.argv[1:], "hnd:o:t:")
    for op, value in opts:
        if op == "-d":
            CREATE_PATH = TEST_PATH + value
        elif op == "-o":
            ORDER = value
        elif op == "-n":
            INSTALL = "NO"
        elif op == "-t":
            if value.isdigit():
                THREAD_NUMBER = value
            else:
                print "Thread number must to be a digit."
                sys.exit(-1)
        elif op == "-h":
            print "basic-test.py -d {test directory} -o {sort | random} -n {no install} -t {thread number}"
            sys.exit(0)
    
    print "Test Path: ", CREATE_PATH
    print "Order: ", ORDER
    print "Thread number: ", THREAD_NUMBER

def main():
    print "Run basic tests on comet. Version: ", VERSION
    #setup test env
    setup_env()
    if CREATE_PATH == "":
        print "No test case directory specified."
        return
    if not os.path.exists(CREATE_PATH):
        print "Test case directory not exist."
        return

    # Setup all threads
    threads =[]
    for i,config in enumerate(configs):
        for j in range(int(THREAD_NUMBER)):
            thread_id = "%d" % j
            t = TestThread(thread_id, "thread"+"-"+config['hostname'], config)
            threads.append(t)

    # Start all threads
    for t in threads:
        t.start()

    # Wait for all of them to finish
    for t in threads:
        t.join()

    generate_report()

if __name__ == "__main__":
    sys.exit(main())

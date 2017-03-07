#-*- coding: UTF-8 -*-
#!/usr/bin/python

import os
from subprocess import call
import fnmatch
import sys
import time
import random
import getopt
import threading
import datetime
from urlparse import urlparse
from docker import Client
from apphouse_sdk import AppHouse
from Logger import Log

JobStatus ={"START":1, "SYNC":2, "REST":3, "SUSPEND":4, "ERROR":0xff}

def date_now():
    return datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')

class DockerClient:
    def __init__(self):
        self.client = Client(base_url='unix://var/run/docker.sock', version='1.22')

    def pull(self, repository, tag='latest'):
        arr = []
        for line in self.client.pull(repository, tag, stream=True):
            arr.append(line)

        #print arr
        return

    def tag(self, image, repository, tag):
        if self.client.tag(image, repository, tag):
            return True
        return False

    def push(self, repository, tag):
        arr = [line for line in self.client.push(repository, tag, stream=True)]
        #print arr
        return 

    def login(self, username, password, email, registry, reauth=True):
        info = self.client.login(username, password, email, registry, reauth)
        #print registry, info

    def remove_image(self, image):
        self.client.remove_image(image, force=True, noprune=False)
        return

class JobConfig:
    def __init__(self, src, dest, namespace):
        self.src = src
        self.src_host=urlparse(src["url"]).hostname
        self.src_domain=self.src_host+":5002"
        self.dest = dest
        self.dest_host=urlparse(dest["url"]).hostname
        self.dest_domain=self.dest_host+":5002"
        self.namespace = namespace
        self.src_ah = AppHouse(src["url"], src["user"],src["password"])
        self.dest_ah = AppHouse(dest["url"], dest["user"],dest["password"])

        self.valid = True
        self.err_message = ""
        if not self.src_ah.login()[0]: #can we login source?
            self.valid = False
            self.err_message = "Failed to login source registry."
        elif not self.src_ah.get_namespace(namespace)[0]: #Do we have namespace in source?
            self.valid = False
            self.err_message = "Failed to get namespace <%s> from source registry."%namespace
        elif not self.dest_ah.login()[0]: #can we login dest?
            self.valid = False
            self.err_message = "Failed to login source registry."
        elif not self.dest_ah.get_namespace(namespace)[0]: #Do we have a namespace in dest?
            r, ns_content = self.src_ah.get_namespace(namespace) # copy namespace first
            if not self.dest_ah.create_namespace(ns_content)[0]:
                self.valid = False
                self.err_message = "Failed to create namespace <%s> to target registry."%namespace
            else:
                self.valid = True


        if not self.valid:
            return 

        self.client = DockerClient()
        self.client.login(src["user"],src["password"],"ignore",self.src_domain)
        self.client.login(dest["user"],dest["password"],"ignore",self.dest_domain)

class SyncJob:
    def __init__(self, config, task):
        self.config = config
        self.start_time = time.ctime(time.time())
        self.status = JobStatus["START"]
        self.name = config.namespace
        
        self.stop_con = threading.Condition()
        self.time_con = threading.Condition()
        self.thread = threading.Thread(target=self.run)
        self.stop = 0
        self.quit = 0
        self.task = task
        self.stat_log = self.task.get_logs()
        self.log_index = len(self.stat_log)

    def get_diff(self):
        is_diff = False
        d = {"add":[],"del":[]}
        l_src = self.config.src_ah.get_repositories_nslist(self.config.namespace)
        #print "src",l_src
        l_dest = self.config.dest_ah.get_repositories_nslist(self.config.namespace)
        #print "dest",l_dest
        for r in set(l_src).symmetric_difference(l_dest):
            is_diff = True
            if r in l_src:
                d["add"].append(r)
            else:
                d["del"].append(r)

        while len(self.task.overwrite) > 0:
            r = self.task.overwrite.pop()
            is_diff = True
            d["add"].append(r)

        return d, is_diff
        
    def add_one_repository(self,repo,tag): #repo is namespace/ubuntu    tag is latest
        log_index = self.log_index

        repository = "%s:%s"%(repo,tag)
        image_src = "%s/%s:%s"%(self.config.src_domain,repo,tag)
        image_dest = "%s/%s:%s"%(self.config.dest_domain,repo,tag)
        repository_src = "%s/%s"%(self.config.src_domain,repo)
        repository_dest = "%s/%s"%(self.config.dest_domain,repo)
        image_dest_tag = "%s/%s:%s"%(self.config.dest_domain,repo,tag)
 
        stat = {
                   "repository":repository,
                   "action":"add",
                   "update_time": date_now(),
                   "status": "in_progress"
        }
        self.stat_log.append(stat)

        Log("---------------------------")
        Log("start pulling %s:%s"%(repository_src,tag))
        self.config.client.pull(repository_src, tag)
        Log("start tag %s to %s:%s"%(image_src, repository_dest, tag))
        self.config.client.tag(image_src, repository_dest, tag)
        Log("start pushing %s:%s"%(repository_dest,tag))
        self.config.client.push(repository_dest, tag)
        Log("start remove local image %s"%(image_src))
        self.config.client.remove_image(image_src)
        Log("start remove local image %s"%(image_dest))
        self.config.client.remove_image(image_dest)
        Log("---------ALL　DONE--------")
        
        datenow = date_now()
        self.stat_log[-1]["status"] = "done"
        self.stat_log[-1]["update_time"] = datenow
        self.task.add_log(self.stat_log[-1],log_index)
        self.log_index += 1

    def del_one_repository(self,repo,tag): #repo is namespace/ubuntu    tag is latest

        log_index = self.log_index
        repository = "%s:%s"%(repo,tag)
        stat = {
                   "repository":repository,
                   "action":"del",
                   "update_time": date_now(),
                   "status": "in_progress"
        }

        self.stat_log.append(stat)

        self.config.dest_ah.delete_one_tag(repo,tag)

        datenow = date_now()
        self.stat_log[-1]["status"] = "done"
        self.stat_log[-1]["update_time"] = datenow
        self.task.add_log(self.stat_log[-1],log_index)
        self.log_index += 1
        Log("Delete repository %s"%repository)

    def check_for_stop(self):
        if self.stop_con.acquire(): #get lock for self.stop
            if self.stop:
                self.stop_con.wait()
            self.stop_con.release()
        return

    def set_for_stop(self):
        if self.stop_con.acquire(): #get lock for self.stop
            self.stop = 1
            self.stop_con.release()
        return

    def resume(self):
        if self.stop_con.acquire(): #get lock for self.stop
            self.stop = 0
            self.stop_con.notify()
            self.stop_con.release()
        return

    def wait_for_call(self):
        if self.time_con.acquire():
            self.time_con.wait(timeout=10)
            self.time_con.release()

    def notify(self):
        if self.time_con.acquire():
            self.time_con.notify()
            self.time_con.release()

    def set_to_quit(self):
        self.quit = 1

    def run(self):
        while not self.quit:
            d, is_diff = self.get_diff()
            if is_diff:
                Log("%s"%str(d))

            for repository in d["add"]:
                if self.quit:
                    break
                self.check_for_stop()
                repo = repository.split(':')[0]
                tag = repository.split(':')[1]
                self.add_one_repository(repo,tag)

            for repository in d["del"]:
                if self.quit:
                    break
                self.check_for_stop()
                repo = repository.split(':')[0]
                tag = repository.split(':')[1]
                self.del_one_repository(repo,tag)

            self.check_for_stop()
            self.wait_for_call()

    def serialize(self):
        pass
        
def main():
    src ={"url":"https://192.168.14.166/api", "user":"admin", "password":"123456"}
    dest ={"url":"https://192.168.14.167/api", "user":"admin", "password":"123456"}
    config = JobConfig(src,dest,"song")
    job = SyncJob(config)
    job.thread.start()

if __name__ == "__main__":
    sys.exit(main())

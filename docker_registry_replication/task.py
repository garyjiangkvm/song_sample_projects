#/bin/python
import datetime
import base64
from sync_job import JobConfig, SyncJob, date_now
from Logger import Log

DEFAULT_USER="replication_master"
DEFAULT_PASSWORD="rm$52306"

class Task:
    def __init__(self,request_json, db):
        self.overwrite = []
        self.namespace = request_json['namespace']
        self.source = request_json['source']
        self.target = request_json['target']
        self.user_id = request_json['user_id']
        self.immediate_start = request_json['immediate_start']
        self.db = db

        if not "uuid" in request_json: # new request
            self.password = base64.b64decode(request_json['password'])
            self.update_time = date_now()
            self.status = "pause"
            self.uuid = base64.encodestring(self.namespace + self.source + self.target)
            self.sync_job = None
            if not db == None: self.add_myself()
        else:
            self.password = request_json['password']
            self.update_time = request_json['update_time']
            self.status = request_json['status']
            self.uuid = request_json['uuid']
            self.sync_job = None

    def add_overwrite(self,repo,tag):
        name = repo + ":" + tag
        if name not in self.overwrite:
            self.overwrite.append(name)
        return name
        
           
    def get_dict(self):
        info = {
            "uuid": self.uuid,
            "namespace": self.namespace,
            "source": self.source,
            "target": self.target,
            "user_id": self.user_id,
            "password": self.password,
            "immediate_start": self.immediate_start,
            "update_time": self.update_time,
            "status": self.status
        }
        return info

    def get_logs(self):
        return self.db.find_logs(self.uuid)

    def add_myself(self):
        self.db.insert_task(self.get_dict())

    def del_myself(self):
        self.db.delete_task({"uuid":self.uuid})

    def add_log(self, log, index):
        log['uuid'] = self.uuid
        log['index'] = index
        tmp = log.copy()  #use copy to remove _id key as it cannot be serialized by jsonif
        self.db.insert_log(tmp)

    def update_log(self, index, status, time):
        self.db.update_log_status(self.uuid, index, status, time)


    def del_log(self):
        log = {}
        log['uuid'] = self.uuid
        self.db.delete_log(log)

    def short_version(self):
       info = {
           "target": self.target,
           "update_time": self.update_time,
           "status": self.status
       }
       return info

    def validate(self):
        src = {
            #"url":"https://%s/api"%self.source,
            "url":"http://127.0.0.1:9182/api",
            "user":DEFAULT_USER,
            "password":DEFAULT_PASSWORD,
        }
        dest = {
            "url":"https://%s/api"%self.target,
            "user":self.user_id,
            "password":self.password,
        }
        config = JobConfig(src,dest,self.namespace)
        return config.valid, config.err_message
        

    def kickoff(self):
        src = {
            #"url":"https://%s/api"%self.source,
            "url":"http://127.0.0.1:9182/api",
            "user":DEFAULT_USER,
            "password":DEFAULT_PASSWORD,
        }
        dest = {
            "url":"https://%s/api"%self.target,
            "user":self.user_id,
            "password":self.password,
        }
        config = JobConfig(src,dest,self.namespace)
        if config.valid:
            self.sync_job = SyncJob(config, self)
            self.sync_job.thread.start()
            self.status = "started"
            self.db.update_task_status(self.uuid, self.status)
        
        return config.valid, config.err_message

    def start(self):
        if self.sync_job == None:
            return self.kickoff()
        else:
            return self.resume()

    def pause(self):
        self.sync_job.set_for_stop()
        self.status = "pause"
        self.db.update_task_status(self.uuid, self.status)

    def resume(self):
        self.sync_job.resume()
        self.status = "started"
        self.db.update_task_status(self.uuid, self.status)
        return True, ""

    def delete(self):
        self.sync_job.set_to_quit()
        self.status = "deleted"
        self.del_myself()
        self.del_log()

    def get_stat(self):
        return self.sync_job.get_stat()

class overwrite:
    def __init__(self):
        pass

class Taskset:
    def __init__(self,db):
        self.tasks = []
        self.ow = []  #overwrite queue to save overwrite repositry
        self.db = db

        self.init_tasks()

    def init_tasks(self):
        Log("Initialise tasks from db...")
        count = 0
        err = 0
        l = self.db.find_tasks(False)
        for doc in l:
            task = Task(doc, self.db)
            self.add(task)
            valid, err_message = task.validate()
            if not valid:
                #we seems can do nothing if it is not valid
                err += 1
                Log("task is not valid on -- %s"%str(doc))
            else: 
                count += 1
                if  task.status == "started":
                    task.kickoff()
        Log("Initialise tasks from db done. count %d err %d"%(count,err))

    def add(self, task):
        self.tasks.append(task)
   
    def add_overwrite(self,ns,repo,tag):
        task_l = filter(lambda x:x.namespace==ns, self.tasks)
        for task in task_l:
            task.add_overwrite(repo,tag)
        return

    def short_version(self,task):
        return task.short_version()

    def get_namespace_tasks(self, ns):
        task_l = filter(lambda x:x.namespace==ns, self.tasks)
        return map(self.short_version,task_l)

    def get_task(self, ns, target):
        for t in self.tasks:
            if t.namespace == ns and t.target == target:
                return t
        return None

    def delete_task(self, task):
        self.tasks.remove(task)


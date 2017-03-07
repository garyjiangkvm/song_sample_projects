#/bin/python
import sys
from pymongo.mongo_client import MongoClient
from pymongo.collection import ReturnDocument


class Dbmgr:
    def __init__(self):
        try:
            self.conn = MongoClient("mongodb://mongodb:27017")
        except Exception,e:
            print e
            self.valid = False
        else:
            self.valid = True
            self.db = self.conn["replication"]
            self.tasks = self.db["tasks"]
            self.logs = self.db["logs"]
            print "Database connected."

    def insert_task(self,record):
        if self.valid:
            return self.tasks.insert_one(record)
        
    def delete_task(self,record):
        if self.valid:
            return self.tasks.delete_one(record)
        
    def insert_log(self,record):
        if self.valid:
            return self.logs.insert_one(record)
        
    def delete_log(self,record):
        if self.valid:
            return self.logs.delete_many(record)

    def find_tasks(self, verbose):
        l = []
        if not self.valid:
            return l 

        cursor = self.tasks.find({})
        for doc in cursor:
            if verbose: print doc
            doc.pop('_id', None)
            l.append(doc)

        return l

    def find_task(self,uuid):
        l = []
        if not self.valid:
            return l

        cursor = self.tasks.find({"uuid": uuid})
        for doc in cursor:
            print doc
            doc.pop('_id', None)
            l.append(doc)

        return l

    def update_task_status(self,uuid,new_status):
        if self.valid:
            doc = self.tasks.find_one_and_update({'uuid': uuid}, {'$set': {'status': new_status}}, return_document=ReturnDocument.AFTER)
        print doc
        
    def update_log_status(self,uuid,index,new_status,new_update_time):
        if self.valid:
            doc = self.tasks.find_one_and_update({'uuid': uuid, 'index': index}, {'$set': {'status': new_status, 'update_time': new_update_time}}, return_document=ReturnDocument.AFTER)
        print doc

    def find_logs(self,uuid):
        l = []
        if not self.valid:
            return l

        cursor = self.logs.find({"uuid": uuid})
        for doc in cursor:
            print doc
            doc.pop('_id', None)
            l.append(doc)
        
        return l
        
    def find_all_logs(self):
        l = []
        if not self.valid:
            return l

        cursor = self.logs.find()
        for doc in cursor:
            print doc
            doc.pop('_id', None)
            l.append(doc)
        
        return l
        
if __name__ == '__main__':
    dbm = Dbmgr()
    #print dbm.conn.server_info()

    if len(sys.argv) == 2 and sys.argv[1] == "drop":
        dbm.tasks.drop()
        dbm.logs.drop()

        sys.exit()

    print
    dbm.find_tasks(True)
    print
    dbm.find_all_logs()
    

'''
    uuid = "c29uZzEyNy4wLjAuMTE5Mi4xNjguMTQuMTY3\n"

    print dbm.find_task(uuid)
    print dbm.update_status(uuid, "started")
    print
    print dbm.find_task(uuid)
'''

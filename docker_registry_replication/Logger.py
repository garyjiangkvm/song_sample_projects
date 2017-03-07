# Copyright (c) 20016-2016 The Cloudsoar.
# See LICENSE for details.

"""
Implement the log manage
"""

from logging import handlers
import logging.config
import os
import threading
import time
import traceback

class Logger:
    def __init__(self):
        return
  
    def get_logger(self,logger_name,log_home,**args):
        log_name = args.get('log_name', "%s.log"%(logger_name))
        log_level = args.get('log_level', logging.DEBUG)
        log_size = args.get('log_size', 10240000)
        backupcount = args.get('backupcount', 10)
        
        log_path = os.path.join(log_home,log_name)        
        if not os.path.isfile(log_path):
            try:
                f = open(log_path,"a")
                f.close()
            except Exception,e:
                traceback.print_exc()
        
        formatter = logging.Formatter("%(asctime)-15s %(levelname)s [%(process)d-%(thread)d] %(message)s")
        handler = handlers.RotatingFileHandler(log_path,"a",log_size,backupcount)
        handler.setFormatter(formatter)
    
        logger = logging.getLogger(logger_name)
        logger.addHandler(handler)
        logger.setLevel(log_level)
        return logger
        
glog = Logger()
logger = glog.get_logger("replication", "/opt/source/Trace/logs")

def Log(msg):
    logger.debug(msg)

if __name__ == '__main__':
    a = {"key0":0, "key1": "1"}
    Log("this is my first %s" % str(a))


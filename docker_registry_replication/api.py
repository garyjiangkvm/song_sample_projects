#/bin/python
from flask import Flask, jsonify, request, abort
from task import Taskset, Task
from db import Dbmgr
from Logger import Log

app = Flask(__name__)

RespCode = {"good":"0", "error":"1", "err_param":"2"}

Good = {
    "content":[],
    "message":"done",
    "result":0
}
ERROR_RESP = {
    "content":[],
    "message":"Error",
    "result":1
}
WRONG_PARAM = {
    "content":[],
    "message":"Wrong parameters given",
    "result":2
}

db = Dbmgr()
taskset = Taskset(db)

def wrong_param():
    resp = jsonify(WRONG_PARAM)
    resp.headers['Result-Code'] = RespCode["err_param"]
    return resp

def good_resp(info):
    resp = jsonify(info)
    resp.headers['Result-Code'] = RespCode["good"]
    return resp

@app.route('/func/replication/config', methods=['GET'])
def get_tasks():
    args = request.args
    ns = args["namespace"]
    task_l = taskset.get_namespace_tasks(ns)
    info = {
        'content':task_l,
        'message': 'done',
        'result': 0
    }
    return good_resp(info)

@app.route('/func/replication/config', methods=['POST'])
def create_task():
    if not request.json or not 'namespace' in request.json:
        return wrong_param()

    task = Task(request.json, db)
    valid, err_message = task.validate()
    if not valid:
        err_resp = ERROR_RESP
        err_resp["message"] = err_message
        resp = jsonify(err_resp)
        resp.headers['Result-Code'] = RespCode["error"]
        return resp
        
    taskset.add(task)
    if task.immediate_start:
        task.kickoff()
        
    return good_resp(Good)

@app.route('/func/replication/testlink', methods=['POST'])
def test_link():
    if not request.json or not 'namespace' in request.json:
        return wrong_param()
    task = Task(request.json, None)
    valid, err_message = task.validate()
    if not valid:
        err_resp = ERROR_RESP
        err_resp["message"] = err_message
        resp = jsonify(err_resp)
        resp.headers['Result-Code'] = RespCode["error"]
        return resp
        
    return good_resp(Good)

@app.route('/func/replication/details', methods=['POST'])
def task_details():
    if not request.json or not 'namespace' in request.json:
        return wrong_param()
    task = taskset.get_task(request.json["namespace"], request.json["target"])
    if task == None:
        return wrong_param()
    info = {
        'content': task.sync_job.stat_log,
        'message': 'done',
        'result': 0
    }
    print info
    return good_resp(info)


@app.route('/func/replication/job', methods=['POST'])
def task_action():
    if not request.json or not 'namespace' in request.json:
        return wrong_param()
    task = taskset.get_task(request.json["namespace"], request.json["target"])
    if task == None:
        return wrong_param()

    action = request.json["action"]
    if action == "start" or action == "resume":
        task.start()
    elif action == "stop":
        task.pause()
    elif action == "delete":
        task.delete()
        taskset.delete_task(task)
    else:
        return wrong_param()
        
    return good_resp(Good)

@app.route('/func/replication/overwrite', methods=['POST'])
def task_overwrite():
    repo = request.json["repository"]
    tag = request.json["tag_name"]
    namespace = repo.split('/')[0]
    taskset.add_overwrite(namespace,repo,tag)
    return good_resp(Good)

if __name__ == '__main__':
    app.run(host='0.0.0.0',debug=True, use_reloader=False, port=9183)

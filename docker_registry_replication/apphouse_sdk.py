#-*- coding: UTF-8 -*-
import base64
import simplejson
import requests
import logging
import time
import hmac
import sha

logging.basicConfig(level=logging.INFO)

logger = logging


class AppHouse:
    def __init__(self, api_url, user="admin", password="admin", repo_base=None):
        if api_url.endswith("/"):
            self.api_url = api_url[:-1]
        else:
            self.api_url = api_url
        if self.api_url[-4:] != "/api":
            self.api_url += "/api"
        logger.info("api url:[%s]", self.api_url)
        self.user = user
        self.password = password
        self.access_key = None
        self.access_uuid = None
        self.repo_base = repo_base

    def get_repo_base(self):
        if self.repo_base is not None:
            if self.repo_base.endswith("/") is False:
                self.repo_base += "/"
            return self.repo_base
        if self.api_url.startswith("http"):
            addr = self.api_url[self.api_url.index("://")+3:].split(":")[0]
            port = 5002
            repo_base = addr+":"+str(port)+"/"
            self.repo_base = repo_base
            return self.repo_base

    def get_token(self, method):
        if self.access_key is None or self.access_uuid is None:
            return None
        timestamp = long(time.time() * 1000)
        msg = "<%d><%s>" % (timestamp, method)
        access_key = str(self.access_key)
        token = dict()
        token["timestamp"] = str(timestamp)
        token["hash"] = base64.encodestring(hmac.new(access_key, msg, sha).hexdigest()).strip()
        token["uuid"] = self.access_uuid
        return base64.encodestring(token["uuid"]).strip(" \r\n")+"%%"+token["timestamp"]+"%%"+token["hash"]

    def make_request(self, uri, data=None, method=None):
        url = self.api_url + uri
        if method is None:
            method = uri.split("/")[1].split("?")[0]
        print "Method is ",method
        logger.info("send to url:[%s]", url)

        token = self.get_token(method=method)
        headers = None
        if token is not None:
            headers = {"token":token}
            logger.info("set header:token:[%s]", token)
        if data is not None:
            logger.info("send data:[%s]", data)
        try:
            if data is None:
                response = requests.get(url, headers=headers, timeout=10, verify=False)
            else:
                response = requests.post(url, json=data, headers=headers, timeout=10, verify=False)
        except requests.exceptions.Timeout as e:
            logger.error("request timeout")
            return None
        except Exception as e:
            logger.error("urlopen error:[%s]", e)
            return None
        try:
            recv = response.json()
        except simplejson.scanner.JSONDecodeError as e:
            logger.error("json decode error:[%s]", e)
            return None
        except Exception as e:
            logger.error("request recv error:[%s]", e)
            return None
        #logger.info("recv data :[%s]", recv)
        logger.info("recv data length:[%s]", len(recv))
        return recv

    def login(self, user=None, password=None):
        if user is None:
            user = self.user
        if password is None:
            password = self.password
        uri = "/login"
        send_info = dict()
        send_info['username'] = base64.encodestring(user).strip(" \r\n")
        send_info['password'] = base64.encodestring(password).strip(" \r\n")
        send_info['__clone__'] = 1
        result = self.make_request(uri, data=send_info)
        if result is None:
            return False, None
        if result['result'] != 0:
            return False, result
        self.access_key = result['content']['access_key']
        self.access_uuid = result['content']['access_uuid']
        return True, result['content']

    def get_ns_repositories(self,ns):
        uri = "/repositories/"+ns+"?__pages_number=9999"
        result = self.make_request(uri)
        if result is None:
            return False, None
        if result['result'] != 0:
            return False, None
        return True, result['content']

    def get_repositories_nslist(self,ns):
        r, all_repo = self.get_ns_repositories(ns)
        repo_list = list()
        if all_repo is None:
            return repo_list
        for repo in all_repo:
            for tag in repo['tags']:
                #print tag['repository'] + ":" + tag['tag_name'] + ":" + tag['digest']
                #repo_list.append(tag['repository'] + ":" + tag['tag_name'] + ":" + tag['digest'])
                repo_list.append(tag['repository'] + ":" + tag['tag_name'])
        return repo_list


    def get_usr_repositories(self):
        uri = "/repositories/usr/"+self.user+"?__pages_number=9999"
        result = self.make_request(uri)
        if result is None:
            return False, None
        if result['result'] != 0:
            return False, None
        return True, result['content']

    def get_repositories_list(self):
        r, all_repo = self.get_usr_repositories()
        repo_list = list()
        if all_repo is None:
            return repo_list
        for repo in all_repo:
            for tag in repo['tags']:
                repo_list.append(self.get_repo_base() + tag['repository'] + ":" + tag['tag_name'])
        return repo_list

    def get_namespace(self,ns):
        uri = "/namespace/"+ns
        result = self.make_request(uri)
        if result is None:
            return False, None
        if result['result'] != 0:
            return False, None
        return True, result['content']
    
    def get_all_tags(self,repo):
        uri = "/repository?repository=%s"%repo
        result = self.make_request(uri)
        if result is None:
            return False, None
        if result['result'] != 0:
            return False, None
        return True, result['content']

    def get_one_tag_info(self,repo,tag):
        r, tags = self.get_all_tags(repo)
        if tags is None:
            return False, None

        info = {}
        for t in tags:
            if t["tag_name"] == tag:
                info["repository"] = t["repository"]
                info["tag_name"] = t["tag_name"]
                info["digest"] = t["digest"]

        return True, info
            
    def delete_one_tag(self,repo,tag):
        r, info = self.get_one_tag_info(repo,tag)
        if info is None:
            return False, None

        uri = "/tag/delete"
        result = self.make_request(uri,data=info,method="delete_tag")
        if result is None:
            return False, None
        if result['result'] != 0:
            return False, None
        return True, result

    def create_namespace(self,ns_content):
        uri = "/namespace/add"
        result = self.make_request(uri,data=ns_content,method="add_namespace")
        if result is None:
            return False, None
        if result['result'] != 0:
            return False, None
        return True, result['content']

if __name__ == "__main__":
    ah = AppHouse("http://127.0.0.1:9182/api", user="admin", password="123456")
    r, content = ah.login()
    print r
    print content

    bh = AppHouse("https://192.168.14.167/api", user="admin", password="123456")
    r, content = bh.login()
    print r
    print content

    '''
    ns_content = ah.get_namespace("song")
    print ns_content
    print bh.create_namespace(ns_content)

    print "-------------------"
    print ah.get_all_tags("song/centos")
    print ah.get_one_tag_info("song/centos","latest")
    print bh.delete_one_tag("song/centos","latest")
    '''

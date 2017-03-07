import sys

class MyLibrary:
    ROBOT_LIBRARY_SCOPE = 'TEST SUITE'

    def Get_Hosts(self, str_all, ip):
        l_hosts= str_all.split("+")
        if ip not in l_hosts:
            return l_hosts
        i = l_hosts.index(ip)
        return l_hosts[i:] + l_hosts[:i]

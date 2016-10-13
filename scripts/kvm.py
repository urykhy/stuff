#!/usr/bin/env python3

import os
import psutil
import shlex
import subprocess
import sys
import yaml

#  start and stop KVM virtual machones, described in yml
#  macaddr must be used in command to detect VM status
#
#  sample kvm.yml
#
"""
version: 1

services:
  centos7-1:
    up: kvm -m 1G -net nic,vlan=1,macaddr=52:62:33:44:56:01,model=virtio -net tap,vlan=1,ifname=vif-c1 -drive cache=unsafe,file=/u01/vm/gfs/centos7-1,if=virtio -nographic
    down: ssh user@centos7-1 sudo poweroff
  cantos7-2:
    up: kvm -m 1G -net nic,vlan=1,macaddr=52:62:33:44:56:02,model=virtio -net tap,vlan=1,ifname=vif-c2 -drive cache=unsafe,file=/u01/vm/gfs/centos7-2,if=virtio -nographic
    down: ssh user@centos7-2 sudo poweroff
"""

state = {}
def update_status(d):
    global state
    state = {}
    test=[]
    for pid in psutil.pids():
        p = psutil.Process(pid)
        l = p.cmdline()
        if len(l) > 1 and l[0]=="qemu-system-x86_64":
            for i in l:
                if i.find("macaddr") != -1:
                    test.append(i)
    services = d["services"]
    for x in services:
        flag = False
        for i in services[x]["up"].split():
            if i in test:
                state[x]=1

def operation_up(d):
    global state
    services = d["services"]
    for x in services:
        if x in state:
            print (x+" already started")
        else:
            print ("starting "+x+" ... ", end="")
            try:
                p = subprocess.Popen(shlex.split(services[x]["up"]), stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).pid
                # FIXME: no error handling here. if communicate is used - then console is garbage after script exit.
                print ("OK")
            except Exception as e:
                print ("Failed:", e)
    return 0

def operation_status(d):
    global state
    for x in d["services"]:
        if x in state:
            print (x,"is running")
        else:
            print (x,"is stopped")
    return 0;

def operation_down(d):
    global state
    services = d["services"]
    for x in services:
        if x in state:
            print ("stopping "+x+" ... ", end="")
            try:
                p = subprocess.Popen(shlex.split(services[x]["down"]), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                o, e = p.communicate(timeout = 2)
                msg = e.decode()
                if msg.find("closed by remote host") == -1:
                    print ("Failed:", msg)
                else:
                    print ("OK");
            except Exception as e:
                print ("Failed:", e)
            p.wait()
        else:
            print (x+" already stopped")
    return 0

if not os.path.exists("kvm.yml"):
    print ("no kvm.yml in current directory")
    sys.exit(0)

with open("kvm.yml", 'r') as stream:
    d = yaml.load(stream)

if int(d["version"]) != 1:
    print ("unsupported kvm.yml version")
    sys.exit(-1)

update_status(d)

if len(sys.argv) == 1:
    sys.exit(operation_status(d))
if sys.argv[1] == "up":
    sys.exit(operation_up(d))
elif sys.argv[1] == "down":
    sys.exit(operation_down(d))

print ("unknown command. only up and down are supported")
sys.exit(-1)


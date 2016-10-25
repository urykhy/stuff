#!/usr/bin/env python3

import os
import psutil
import shlex
import subprocess
import sys
import yaml

#  start and stop KVM virtual machines, described in yml
#  macaddr must be used in command to detect VM status

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

def operation_up(d, arg):
    global state
    services = d["services"]
    for x in services:
        if len(arg) > 0 and x not in arg:
            continue
        if x in state:
            print (x+" already started")
        else:
            print ("starting "+x+" ... ", end="")
            try:
                args = ["nohup"]
                args.extend(shlex.split(services[x]["up"]))
                p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.DEVNULL, close_fds=True)
                o, e = p.communicate(timeout = 2)
                msg = e.decode()
                print ("Failed:", msg)
                #p = subprocess.Popen(shlex.split(services[x]["down"]), stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, close_fds=True).pid
            except subprocess.TimeoutExpired:
                print ("OK")
            except Exception as e:
                print ("Failed:", e)
                p.wait()
    return 0

def operation_status(d):
    global state
    for x in d["services"]:
        if x in state:
            print (x,"is running")
        else:
            print (x,"is stopped")
    return 0;

def operation_down(d, arg):
    global state
    services = d["services"]
    for x in services:
        if len(arg) > 0 and x not in arg:
            continue
        if x in state:
            print ("stopping "+x+" ... ", end="")
            try:
                p = subprocess.Popen(shlex.split(services[x]["down"]), stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
                o, e = p.communicate(timeout = 2)
                msg = e.decode()
                if msg.find("closed by remote host") == -1 and len(msg) > 1:
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
    sys.exit(operation_up(d, sys.argv[2:]))
elif sys.argv[1] == "down":
    sys.exit(operation_down(d, sys.argv[2:]))

print ("unknown command. only up and down are supported")
sys.exit(-1)


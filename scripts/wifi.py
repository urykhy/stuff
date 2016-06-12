#!/usr/bin/env python

#
# WiFi client
# set dev,config,wpa_config parameters in script and go as root.
#
#
# setup /etc/network/interfaces:
#
# iface wifi inet dhcp
#    wpa-conf /etc/wpa_supplicant/client.conf


import re
import argparse
import os
import subprocess

dev="wlan1"
config="/home/ury/.config/wifi.conf"
wpa_config="/etc/wpa_supplicant/client.conf"

def write_config(ssid, password):
    f = open(wpa_config, "w")
    print >> f, "ctrl_interface=/var/run/wpa_supplicant"
    print >> f, "ctrl_interface_group=0"
    print >> f, "eapol_version=1"
    print >> f, "ap_scan=1"
    print >> f, "fast_reauth=1"
    print >> f, "network={"
    print >> f, "ssid=\""+ssid+"\""
    print >> f, "scan_ssid=1"
    print >> f, "psk=\""+password+"\""
    print >> f, "}"
    print "Config written to", wpa_config

def list_known():
    try:
        print "Known networks"
        for line in open(config, "r"):
            line = line.strip()
            u,p = line.split(':')
            print " ",u
    except:
        pass

def add_new(a):
    f = open(config, "a")
    print >> f, a
    print "Added new network:",a

def up():
    print "ifup ... "
    subprocess.call(["/sbin/ifup",dev+"=client"])

def connect(a):
    try:
        for line in open(config, "r"):
            line = line.strip()
            u,p = line.split(':')
            if u == a:
                write_config(u, p)
                up()
                return
        print "Network ",a,"not found"
    except Exception as e:
        print e


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-a", help="add network", type=str, metavar="ssid:password")
    parser.add_argument("-c", help="connect", metavar="ssid")
    args = parser.parse_args()
    if args.a:
        add_new(args.a)
    elif args.c:
        connect(args.c)
    else:
        list_known()
        print
        parser.print_help();


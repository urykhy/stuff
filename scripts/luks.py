#!/usr/bin/python3

#
# LUKS mounting script
# configured via /etc/crypttab and fstab
# need sudo
#

import re
import argparse
import os
import subprocess

fstab = {}
crypttab = {}
dev_prefix = "/dev/mapper/"


def read_info():
    global crypttab
    global fstab
    for i in [line.strip() for line in open("/etc/crypttab", "r")]:
        if len(i) == 0 or i[0] == "#":
            continue
        i = re.split("\\s+", i)
        if "luks" in i[3]:
            if i[1].startswith("UUID="):
                i[1] = os.path.realpath("/dev/disk/by-uuid/" + i[1][len("UUID=") :])
            # print (i[0], ":", i[1:])
            crypttab[i[0]] = i[1:]

    for i in [line.strip() for line in open("/etc/fstab", "r")]:
        if len(i) == 0 or i[0] == "#":
            continue
        i = re.split("\\s+", i)
        if i[0].startswith(dev_prefix):
            # print (i[0][len(dev_prefix):], ":", i[1])
            fstab[i[0][len(dev_prefix) :]] = i[1]
        elif i[0].startswith("UUID="):
            dev_name = os.path.realpath("/dev/disk/by-uuid/" + i[0][len("UUID=") :])
            fstab[dev_name] = i[1]
            # print (dev_name, ":", i[1])


def print_status():
    global crypttab
    global fstab
    for i, j in sorted(crypttab.items()):
        if not os.path.exists(j[0]):
            print("%s: not connected" % (i))
        elif not os.path.exists(dev_prefix + i):
            print("%s: connected" % (i))
        elif i in fstab and not os.path.ismount(fstab[i]):
            print("%s: connected, dm started" % (i))
        else:
            print("%s: connected, dm started, mounted" % (i))


def run_mount(i):
    global crypttab
    global fstab
    print("mounting", i)
    dev_name = crypttab[i][0]
    luks_key = crypttab[i][1]
    opts = crypttab[i][2]
    if not os.path.exists(dev_name):
        print("%s: not connected" % (i))
    else:
        if not os.path.exists(dev_prefix + i):
            args = ["sudo", "/sbin/cryptsetup", "-T", "1", "luksOpen", dev_name, i]
            if luks_key != "none" and os.path.exists(luks_key):
                args.extend(["--key-file", luks_key])
            subprocess.call(args)
        if not os.path.ismount(fstab[i]):
            subprocess.call(["sudo", "/bin/mount", fstab[i]])
        subprocess.call(["/bin/df", "-h", fstab[i]])


def run_umount(i):
    global crypttab
    global fstab
    print("umounting", i)
    dev_name = crypttab[i][0]
    luks_key = crypttab[i][1]
    opts = crypttab[i][2]
    if os.path.ismount(fstab[i]):
        subprocess.call(["sudo", "/bin/umount", fstab[i]])
    if os.path.exists(dev_prefix + i):
        subprocess.call(["sudo", "/sbin/cryptsetup", "luksClose", i])


def run_disconnect(i):
    global crypttab
    global fstab
    print("disconnecting", i)
    dev_name = crypttab[i][0]
    luks_key = crypttab[i][1]
    opts = crypttab[i][2]
    if not os.path.exists(dev_name):
        print("%s: not connected" % (i))
    elif not os.path.exists(dev_prefix + i):
        dName = os.path.basename(os.path.realpath(dev_name))
        subprocess.call(
            [
                "sudo",
                "/bin/sh",
                "-c",
                "/bin/echo 1 > /sys/block/" + dName + "/device/delete",
            ]
        )
    else:
        print("%s: in use" % (i))


if __name__ == "__main__":
    read_info()
    parser = argparse.ArgumentParser()
    parser.add_argument("-m", help="mount by cryptodisk name")
    parser.add_argument("-u", help="umount by cryptodisk name")
    parser.add_argument("-d", help="disconnect by cryptodisk name, (umount first)")
    args = parser.parse_args()

    if args.m:
        run_mount(args.m)
    elif args.u:
        run_umount(args.u)
    elif args.d:
        run_disconnect(args.d)
    else:
        print_status()

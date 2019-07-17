#!/usr/bin/python3

# only bash shell supported
# since we need -o pipefail option

import shlex
import subprocess
import os
import sys
import yaml
import argparse

parser = argparse.ArgumentParser(description='backup manager')
parser.add_argument('-d','--destination', type=str,                         help='backup to specified destination')
parser.add_argument('-c','--config',      type=str, default='backuper.yml', help='specify yaml config file')
parser.add_argument('--parse',            action='store_true',              help='print parsed config')
parser.add_argument('--dry-run',          action='store_true',              help='print command to be executed and exit')
args = parser.parse_args()

dest = args.destination

with open(args.config, 'r') as f:
    cfg = yaml.load(f)

if args.parse:
    print (cfg)
    sys.exit(0)

if dest is None:
    print ("destination not specified")
    sys.exit(-1)

def format_cmd(cmds, xd, xs):
    # cmds: list with commands
    # xd: src folder params
    # xs: short src folder name
    x = cfg['destinations'][dest]
    s = xd['path']
    f = None

    # decode path
    s = subprocess.check_output('bash -c "echo '+s+'"', shell=True).strip().decode("utf-8")

    if dest in cfg['fixtures']:
        f = cfg['fixtures'][dest]
        if xs in f:
            f = f[xs]
        else:
            f = None

    if f == "tar.zst":
        cmds.append("sudo tar -czf - {} | zstd -6 -T6 > /tmp/{}.tar.zst".format(s,xs))
        s = "/tmp/{}.tar.zst".format(xs)

    if dest == "restic":
        c = "{} --tag {} {}".format(x['cmd'], xd['tag'], s)
    elif dest == "tar":
        xdot = "."
        cd = "-C"
        if f == "no_dot":
            xdot = ""
        elif f == "no_cd":
            xdot = ""
            cd = ""
        c="{} {} {} {} {}{}{}".format(x['cmd'], cd, s, xdot, x['destination'], xs, x['suffix'])
    elif dest == "flash":
        c="{} {} {}".format(x['cmd'], s, x['destination'])
    else:
        c="{} {} {}{}".format(x['cmd'], s, x['destination'], xs)

    if "sudo" in xd and xd['sudo']:
        cmds.append("sudo " + c)
    else:
        cmds.append(c)

    if f == "tar.zst":
        cmds.append("rm {}".format(s))

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def perform(cmds, xs):
    # cmds: list with commands
    # xs:   short folder name
    for c in cmds:
        print (bcolors.OKBLUE+"{:>12s}".format(xs)+bcolors.WARNING,"running..."+bcolors.ENDC, end = '')
        sys.stdout.flush()

        p = subprocess.Popen("set -e; set -o pipefail; "+c, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if p.wait() == 0:
            print ('\r'+bcolors.OKBLUE+"{:>12s}".format(xs)+bcolors.OKGREEN,"success.   "+bcolors.ENDC)
        else:
            print ('\r'+bcolors.OKBLUE+"{:>12s}".format(xs)+bcolors.FAIL,"error.     "+bcolors.ENDC)
        out = p.stdout.read()
        if 'log' in cfg and len(out) > 0:
            with open(cfg['log']+"_"+xs, "ab") as f:
                f.write(out)

cmds = []
for x in cfg['folders']:
    xd = cfg['folders'][x]
    if dest in xd['destinations']:
        format_cmd(cmds, xd, x)
        if args.dry_run:
            for c in cmds:
                print (c)
        else:
            perform(cmds, x)
        cmds = []

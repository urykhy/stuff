#!/usr/bin/env python

import subprocess
import json
import sys
import time

if len(sys.argv) == 1:
    ZONE="hadoop"
else:
    ZONE=sys.argv[1]
print ZONE

f = open("/etc/bind/master/" + ZONE, 'w')
# print header
print >>f, """$ORIGIN .
$TTL 3600       ; 1 hour"""
print >>f, ZONE + """           IN SOA  ns.dark. ury.elf.dark. (
                                """+ str(int(time.time())) +""" ; serial
                                3600       ; refresh (1 hour)
                                600        ; retry (10 minutes)
                                86400      ; expire (1 day)
                                3600       ; minimum (1 hour)
                                )

                                NS      ns.dark.
$ORIGIN """ + ZONE + """
"""

# print actual data
l = json.loads(subprocess.check_output(["docker", "network","inspect",ZONE]))
cont = l[0]["Containers"]
for i in cont:
    print >>f, cont[i]["Name"].replace("_","-"), "A", cont[i]["IPv4Address"].split("/")[0]
f.close()

subprocess.call(["rndc","reload"])

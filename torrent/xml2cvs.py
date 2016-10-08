#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# convert new xml based backup to old format
#

import xmltodict
import glob
import sys
import os.path
import csv
import datetime

backup_path = "/u01/common/"
csv_path = "/u03/mirror/rutracker-torrents"
buf=""

def parse_one(buf):
    global forum2cat_id
    global cat2name
    root = xmltodict.parse(buf)["torrent"]
    hash = root["magnet"].split(":")[3]
    hash = hash[:hash.index("&tr")]
    forum_id = int(root["forum"]["@id"])
    forum = root["forum"]["#text"]
    id = root["@id"]
    name = root["title"]
    size = root["@size"]
    date = root["@registred_at"]
    date = datetime.datetime.strptime(date, "%Y.%m.%d %H:%M:%S")
    date = str(date)
    try:
        cat_id = forum2cat_id[forum_id]
    except:
        print ("no category for forum",forum_id,"torrent id",id)
        return []
    category = cat2name[cat_id]
    u=[forum_id, forum, id, hash, name, size, date]
    return [cat_id,u]

def write_out(r):
    global cat2writer
    if len(r) == 2:
        cat2writer[r[0]].writerow(r[1])

cat2name={}
print ("read category_info.csv")
reader = csv.reader(open("lostandfound/category_info.csv", 'r'), delimiter=';', quotechar='\"')
for [cat_id,cat_name,fname] in reader:
    cat2name[int(cat_id)]=cat_name

forum2cat_id={}
print ("read forums.csv")
reader = csv.reader(open("lostandfound/forums.csv", 'r'), delimiter=';', quotechar='\"')
for [forum_id,forum_name,cat_id] in reader:
    try:
        forum2cat_id[int(forum_id)]=int(cat_id)
    except:
        pass

namelist = sorted(glob.glob(buckup_path + "/backup.*.xml"), key = lambda s: int(os.path.basename(s).split(".")[1]), reverse=True)
fname = namelist[0]

ts = os.path.basename(fname).split(".")[1][:8]
csv_path = os.path.join(csv_path, ts)
print ("destination with with timestamp: ",csv_path)
if not os.path.exists(csv_path):
    os.mkdir(csv_path)

cat2writer={}
print ("create csv writers")
for x in cat2name:
    cat2writer[int(x)] = csv.writer(open(os.path.join(csv_path, "category_"+str(x)+".csv"), 'w', encoding='utf-8'), delimiter=';', quotechar='\"')

print ("reading backup",fname)
with open(fname) as f:
    for line in f:
        line = line.strip()
        buf += line
        if line == "</torrent>":
            write_out(parse_one(buf))
            buf=""


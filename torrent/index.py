#!/usr/bin/env python3

#
# Licensed under terms of JSON license
# http://www.json.org/license.html
# (c) urykhy
#

import os, sys, string, codecs
import csv

from elasticsearch import Elasticsearch
from elasticsearch import helpers

cache="/tmp/torrent_cache"
home="/root/rutracker-torrents" # absolute path required
info="category_info.csv"
ela_host="localhost"
ela_index="tracker-index"
ela_doc="torrent"
threads=2
timeout=600

es = Elasticsearch([ela_host])
es.indices.delete(index=ela_index, ignore=[400, 404])
es.indices.create(index=ela_index, ignore=400, body={
    'settings': {
        'number_of_shards': 1,
    },
    ela_doc : {
        '_source': {'enabled': 'false'},
        '_all':    {'enabled': 'false'},
        'properties': {
            'category': {'type': 'string'},
            'forum': {'type': 'string'},
            'id':    {'type': 'long'},
            'hash':  {'type': 'string','index' : 'not_analyzed'},
            'name':  {'type': 'string'},
            'size': {'type': 'long',  'index' : 'not_analyzed'},
            'data': {'type': 'date',  'format': 'yyyy-MM-dd hh:mm:ss'}
        }
    }
})

def fname2cache(f):
    tn = f[len(home)+1:].replace('/','_')
    return os.path.join(cache,tn)

def easy_parallize(f, sequence):
    from multiprocessing import Pool
    #from multiprocessing.dummy import Pool
    pool = Pool(processes=threads)
    pool.map(f, sequence)
    pool.close()
    pool.join()

csv_list = []
folders = [f for f in os.listdir(home) if os.path.isdir(os.path.join(home, f))]
for d in folders:
    print("process directory",d)
    d = os.path.join(home,d)
    reader = csv.reader(open(os.path.join(d, info), 'r'), delimiter=';', quotechar='\"')
    for [cat_id,cat_name,fname] in reader:
        #cat_name = cat_name.decode(sys.stdin.encoding)
        fname = os.path.join(d, fname)
        csv_list.append([cat_id,cat_name, fname])

def f(a):
    [cat_id,cat_name,fname] = a
    if os.path.exists(fname2cache(fname)):
        print("already processed:",fname)
        return
    #print("reading:",fname,"for",cat_name)
    t = csv.reader(open(os.path.join(d, fname), 'r'), delimiter=';', quotechar='\"')
    actions = []
    for a in t:
        [forum_id, forum_name, id, hash, name, size, *date] = a
        doc = {
                'category' : cat_name,
                'forum'    : forum_name,
                'id'   : int(id),
                'hash' : hash,
                'name' : name,
                'size' : int(size),
                'date' : date
        }
        action = {
                "_index": ela_index,
                "_type": ela_doc,
                "_id": id,
                "_body": doc,
                "_timeout": timeout
        }
        actions.append(action)
    es_ = Elasticsearch([ela_host])
    helpers.bulk(es_, actions)
    print("done:",fname)
    #open(fname2cache(fname), 'a').close()

if not os.path.exists(cache):
    os.mkdir(cache)

print("about to process",len(csv_list),"files")
easy_parallize(f, csv_list)

print("refresh index...")
es.indices.refresh(index=ela_index)
print("done!")


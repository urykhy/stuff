#!/usr/bin/env python3

#
# Licensed under terms of JSON license
# http://www.json.org/license.html
# (c) urykhy
#

import os, sys, string, codecs
import csv
import re

from elasticsearch import Elasticsearch
from elasticsearch import helpers

home="/u01/mirror/rutracker-torrents" # absolute path required
info="category_info.csv"
ela_host="test"
ela_index="tracker-multi-index"
ela_doc="torrent"
threads=2
timeout=600

def easy_parallize(f, sequence):
    from multiprocessing import Pool
    #from multiprocessing.dummy import Pool
    pool = Pool(processes=threads)
    pool.map(f, sequence)
    pool.close()
    pool.join()

def get_index_name(f):
    return ela_index + "-" + re.search('_(\d+)\.', f).group(1)

def new_index(es, name):
    print ("create index",name)
    es.indices.delete(index=name, ignore=[400, 404])
    es.indices.create(index=name, ignore=400, body={
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
                'data': {'type': 'date',  'format': 'yyyy-MM-dd hh:mm:ss', 'index' : 'not_analyzed'}
            }
        }
    })

def f(a):
    [cat_id,cat_name,fname, index_name] = a
    t = csv.reader(open(fname, 'r'), delimiter=';', quotechar='\"')
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
                "_index": index_name,
                "_type" : ela_doc,
                "_id"   : id,
                "_body" : doc,
                "_timeout": timeout
        }
        actions.append(action)
    es_ = Elasticsearch([ela_host])
    helpers.bulk(es_, actions)
    print("done:",fname)

#
# looks like latest directory contain all `alive` torrents,
# so do not mess with old data
#

folders = [f for f in os.listdir(home) if os.path.isdir(os.path.join(home, f))]
[last_dir] = sorted(folders)[-1:]
print ("process latest directory:",last_dir)

csv_list = []
index_list = []
d = os.path.join(home,last_dir)
reader = csv.reader(open(os.path.join(d, info), 'r'), delimiter=';', quotechar='\"')
for [cat_id,cat_name,fname] in reader:
    index = get_index_name(fname)
    index_list.append(index)
    fname = os.path.join(d, fname)
    csv_list.append([cat_id,cat_name, fname, index])

es = Elasticsearch([ela_host])
print("recreate indexes")
for index in index_list:
    new_index(es, index)

print("about to process",len(csv_list),"files")
easy_parallize(f, csv_list)

print("optimize indexes")
for index in index_list:
    es.indices.optimize(index=index)
print("done!")


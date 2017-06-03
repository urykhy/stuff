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

csv_path="/u03/mirror/rutracker-torrents" # absolute path required
ela_host="elasticsearch.elk"
ela_index="tracker-multi-index"
ela_doc="torrent"
threads=4
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
    es.indices.delete(index=name, ignore=[400, 404])
    es.indices.create(index=name, ignore=400, body={
        'settings': {
            'number_of_shards': 1,
        },
        "mappings": {
            ela_doc : {
                '_source': {'enabled': 'true'},
                '_all':    {'enabled': 'false'},
                'properties': {
                    'category': {'type': 'string'},
                    'forum': {'type': 'string'},
                    'id':    {'type': 'long', 'index' : 'not_analyzed'},
                    'hash':  {'type': 'string', 'index' : 'not_analyzed'},
                    'name':  {'type': 'string'},
                    'size': {'type': 'long', 'index' : 'not_analyzed'},
                    'date': {'type': 'date', 'format': 'yyyy-MM-dd HH:mm:ss', 'index' : 'not_analyzed'}
                }
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
                '_id'  : int(id),
                'category' : cat_name,
                'forum'    : forum_name,
                'id'   : int(id),
                'hash' : hash,
                'name' : name,
                'size' : int(size),
                'date' : date
        }
        actions.append(doc)
    es = Elasticsearch([ela_host], timeout=timeout)
    new_index(es, index_name)
    helpers.bulk(es, actions, index=index_name, doc_type=ela_doc)
    print("done:",fname)

#
# looks like latest directory contain all `alive` torrents,
# so do not mess with old data
#

folders = [f for f in os.listdir(csv_path) if os.path.isdir(os.path.join(csv_path, f))]
[last_dir] = sorted(folders)[-1:]
print ("process latest directory:",last_dir)

csv_list = []
index_list = []
d = os.path.join(csv_path,last_dir)
reader = csv.reader(open("lostandfound/category_info.csv", 'r'), delimiter=';', quotechar='\"')
for [cat_id,cat_name,fname] in reader:
    index = get_index_name(fname)
    index_list.append(index)
    fname = os.path.join(d, fname)
    csv_list.append([cat_id,cat_name, fname, index])

print("about to process",len(csv_list),"files")
easy_parallize(f, csv_list)
print("done!")


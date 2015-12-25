#!/usr/bin/env python

#
# Licensed under terms of JSON license
# http://www.json.org/license.html
# (c) urykhy
#

import os, sys, string, codecs, re
import zipfile,shutil
from elasticsearch import Elasticsearch
from elasticsearch import helpers

ela_host="test"
ela_index="fb2-index"
ela_doc="fb2"
timeout=600
threads=3

#mirror_path = "/u02/mirror/fb2.Flibusta.Net"
mirror_path = "/home/ury/tmp"

from multiprocessing.dummy import Pool
pool = Pool(processes=threads)

def index_name(f):
    m = re.search('.*(-\d+-\d+)\.inp', f)
    return ela_index + m.group(1)

def new_index(es, name):
    #print "create index",name
    es.indices.delete(index=name, ignore=[400, 404])
    es.indices.create(index=name, ignore=400, body={
        'settings': {
            'number_of_shards': 1,
        },
        ela_doc : {
            '_source': {'enabled': 'false'},
            '_all':    {'enabled': 'false'},
            'properties': {
                'author': {'type': 'string'},
                'title': {'type': 'string'},
                'id':    {'type': 'long'}
            }
        }
    })

def indexer(fname, books):
    index_name_ = index_name(fname)
    es = Elasticsearch([ela_host])
    new_index(es, index_name_)
    actions = []
    for a in books:
        [author, title, id] = a
        doc = {
                'author' : author,
                'title'  : title,
                'id'     : int(id)
        }
        action = {
                "_index": index_name_,
                "_type" : ela_doc,
                "_id"   : id,
                "_body" : doc,
                "_timeout": timeout
        }
        actions.append(action)
    helpers.bulk(es, actions)
    es.indices.optimize(index=index_name_)
    print "done:",fname

def read_inp(z,fname):
    print "read",fname
    books = []
    with z.open(fname) as f:
        for l in f:
            l=l.strip().decode(sys.stdin.encoding)
            (au, genre, name, seq, _None, id, _None) = l.split("\04",6)
            if len(seq):
                books.append((au, name + '/' + seq, id))
            else:
                books.append((au, name, id))
    pool.apply_async(indexer, [fname, books])

with zipfile.ZipFile(mirror_path + "/flibusta_fb2_local.inpx") as zfile:
    for info in zfile.infolist():
        if info.filename.endswith('.inp'):
            read_inp(zfile, info.filename)
pool.close()
pool.join()

print("done!")



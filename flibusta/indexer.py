#!/usr/bin/env python3

#
# Licensed under terms of JSON license
# http://www.json.org/license.html
# (c) urykhy
#

import os, sys, string, codecs, re
import zipfile,shutil
from elasticsearch import Elasticsearch
from elasticsearch import helpers

ela_host="elasticsearch.elk"
ela_index="fb2-index"
ela_doc="fb2"
timeout=600
threads=6

files_to_process = 0
files_read = 0

mirror_path = "/u03/mirror/fb2.Flibusta.Net"

from multiprocessing.dummy import Pool
pool = Pool(processes=threads)

def index_name(f):
    m = re.search('.*(-\d+-\d+)\.inp', f)
    return ela_index + m.group(1)

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
                    'author': {'type': 'string'},
                    'title': {'type': 'string'},
                    'id':    {'type': 'long', 'index' : 'not_analyzed'},
                    'size':  {'type': 'long', 'index' : 'not_analyzed'},
                    'date':  {'type': 'date', 'index' : 'not_analyzed', 'format': 'yyyy-MM-dd'}
                }
            }
        }
    })

def indexer(fname, books):
    index_name_ = index_name(fname)
    actions = []
    for a in books:
        [author, title, id, size, date] = a
        doc = {
                "_id"    : int(id),
                'author' : author,
                'title'  : title,
                'id'     : int(id),
                'size'   : int(size),
                'date'   : date
        }
        actions.append(doc)
    es = Elasticsearch([ela_host], timeout=timeout)
    new_index(es, index_name_)
    helpers.bulk(es, actions, index=index_name_, doc_type=ela_doc)
    global files_read
    files_read += 1
    progress = files_read / float(files_to_process)
    print ("\rIndexing: [{0:50s}] {1:.1f}%".format('#' * int(progress * 50), progress * 100), end="")

def read_inp(z,fname):
    books = []
    with z.open(fname) as f:
        for l in f:
            l=l.strip().decode(sys.stdin.encoding)
            # http://forum.home-lib.net/index.php?showtopic=16
            # AUTHOR;GENRE;TITLE;SERIES;SERNO;LIBID;SIZE;FILE;DEL;EXT;DATE;LANG;LIBR ATE;KEYWORDS;
            (au, genre, name, seq, _None, id, size, _None, f_del, _None, date, _None) = l.split("\04",11)
            if f_del == "0":
                au = au.rstrip(":,-")
                au = au.replace(',',", ")
                if len(seq):
                    books.append((au, seq + '/' + name, id, size, date))
                else:
                    books.append((au, name, id, size, date))
    pool.apply_async(indexer, [fname, books])

with zipfile.ZipFile(mirror_path + "/flibusta_fb2_local.inpx") as zfile:
    for info in zfile.infolist():
        if info.filename.endswith('.inp'):
            files_to_process+=1
    for info in zfile.infolist():
        if info.filename.endswith('.inp'):
            read_inp(zfile, info.filename)
pool.close()
pool.join()

print("done!")



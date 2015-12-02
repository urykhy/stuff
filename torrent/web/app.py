#!/usr/bin/python3
# vim:ts=4:sts=4:sw=4:et
# -*- coding: utf-8 -*-

#
# Licensed under terms of JSON license
# http://www.json.org/license.html
# (c) urykhy
#

import string
import cherrypy
import os

_home = os.path.dirname(os.path.abspath(__file__))

from elasticsearch import Elasticsearch
from elasticsearch_dsl import Search, Q
ela_host="test"
ela_index="tracker-multi-index*"
es = Elasticsearch([ela_host])

def human2bytes(s):
    symbols = ('B', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y')
    letter = s[-1:].strip().upper()
    num = s[:-1]
    assert num.isdigit() and letter in symbols
    num = float(num)
    prefix = {symbols[0]:1}
    for i, s in enumerate(symbols[1:]):
        prefix[s] = 1 << (i+1)*10
    return int(num * prefix[letter])

class App(object):
    _cp_config = {
        'tools.staticdir.on' : True,
        'tools.staticdir.dir' : _home,
    }
    def __init__(self):
        pass

    @cherrypy.expose("Home")
    def index(self):
        raise cherrypy.HTTPRedirect("index.html")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def search(self, **params):
        limit_cat = params.get('cat', "").strip()
        limit_forum = params.get('forum', "").strip()
        limit_count = int(params.get('count', 10))
        limit_size_min = human2bytes(params.get('min', "0b"))
        limit_size_max = human2bytes(params.get('max', "0b"))
        limit_wild = int(params.get('wild', 0))
        arg = params.get('query', '').strip()
        if not arg:
            arg = "hobbit"

        s = Search(using=es, index=ela_index)
        if limit_size_min:
            s = s.filter("range", size = {'gte' : limit_size_min })
        if limit_size_max:
            s = s.filter("range", size = {'lte' : limit_size_max })

        arg = arg.split(' ')
        if limit_wild:
            q = Q("wildcard", name="*"+arg.pop(0)+"*")
            for a in arg:
                q = q & Q("wildcard", name="*"+a+"*")
        else:
            q = Q("match", name=arg.pop(0))
            for a in arg:
                q = q & Q("match", name=a)

        if len(limit_cat):
            for a in limit_cat.split(' '):
                q = q & Q("match", category=a)
        if len(limit_forum):
            for a in limit_forum.split(' '):
                q = q & Q("match", forum=a)

        s = s.query(q)
        r = s.execute()
        size = r.hits.total
        if size > limit_count:
            size = limit_count
        s = s.sort('-size')
        s = s.extra(size=size)
        r = s.execute()

        data = []
        for hit in r:
            b = hit._body
            a = [b.id, b.size, b.name, b.category, b.forum, b.date[0] if b.date else '', b.hash]
            data.append(a)

        return {'data': data}

if __name__ == '__main__':
    cherrypy.quickstart(App())
else:
    cherrypy.config.update({'environment': 'embedded'})
    application = cherrypy.Application(App(), script_name=None, config=None)


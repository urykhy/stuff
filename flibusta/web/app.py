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
from cherrypy.lib.static import serve_download
import re
from os.path import isfile, join
import zipfile,shutil

_home = os.path.dirname(os.path.abspath(__file__))

from elasticsearch import Elasticsearch
from elasticsearch_dsl import Search, Q
ela_host="test"
ela_index="fb2-index*"
es = Elasticsearch([ela_host])

mirror_path = "/u02/mirror/fb2.Flibusta.Net"
tmp_path = "/tmp"

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

    def split_name(self, f):
        m = re.search('.*-(\d+)-(\d+)\.zip', f)
        return (int(m.group(1)),int(m.group(2)))

    @cherrypy.expose
    def download(self, **params):
        id = int(params.get('id', "0").strip())
        if id == 0:
            pass
        xname = str(id) + ".fb2"
        tname = join(tmp_path,xname)
        onlyfiles = [f for f in os.listdir(mirror_path) if isfile(join(mirror_path, f))]
        for f in onlyfiles:
            if f.endswith(".zip"):
                (x1,x2) = self.split_name(f)
                if x1 <= id and id <= x2:
                    #cherrypy.log("book must be in "+f)
                    with zipfile.ZipFile(join(mirror_path, f)) as z:
                        with z.open(xname) as zf, open(tname, 'wb') as f:
                            shutil.copyfileobj(zf, f)
                            return serve_download(tname)

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def search(self, **params):
        limit_author = params.get('author', "").strip()
        limit_title = params.get('title', "").strip()
        limit_count = int(params.get('count', 10))
        limit_wild = int(params.get('wild', 0))
        q = None

        if not limit_author and not limit_title:
            limit_title = "hobbit"

        s = Search(using=es, index=ela_index)
        arg = limit_title.split(' ')
        arg = [x for x in arg if x]
        if len(arg):
            if limit_wild:
                q = Q("wildcard", title="*"+arg.pop(0)+"*")
                for a in arg:
                    q = q & Q("wildcard", title="*"+a+"*")
            else:
                q = Q("match", title=arg.pop(0))
                for a in arg:
                    q = q & Q("match", title=a)

        arg = limit_author.split(' ')
        arg = [x for x in arg if x]
        if len(arg):
            for a in arg:
                if q:
                    q = q & Q("match", author=a)
                else:
                    q = Q("match", author=a)

        s = s.query(q)
        #cherrypy.log("query is "+str(s.to_dict()))
        r = s.execute()
        size = r.hits.total
        if size > limit_count:
            size = limit_count
        s = s.sort('-id')
        s = s.extra(size=size)
        r = s.execute()
        #cherrypy.log("result is "+str(r))

        data = []
        for hit in r:
            b = hit._body
            a = [b.id, b.author, b.title]
            data.append(a)

        return {'data': data}

if __name__ == '__main__':
    cherrypy.quickstart(App())
else:
    cherrypy.config.update({'environment': 'embedded'})
    application = cherrypy.Application(App(), script_name=None, config=None)


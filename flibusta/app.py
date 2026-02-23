#!/usr/bin/python3
# vim:ts=4:sts=4:sw=4:et
# -*- coding: utf-8 -*-

#
# Licensed under terms of JSON license
# http://www.json.org/license.html
# (c) urykhy
#

import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.dirname(SCRIPT_DIR))

import re
import shutil
import string
import zipfile
from os.path import isfile, join

import cherrypy
from cherrypy.lib.static import serve_download
from sqlalchemy import bindparam, select, text

from flibusta.common import *

_home = os.path.dirname(os.path.abspath(__file__))

mirror_path = "/u03/mirror/fb2.Flibusta.Net"
tmp_path = "/tmp"


class App(object):
    _cp_config = {
        "tools.staticdir.on": True,
        "tools.staticdir.dir": _home,
        "request.show_tracebacks": True,
    }

    def __init__(self):
        self.engine = engine()
        pass

    @cherrypy.expose("Home")
    def index(self):
        raise cherrypy.HTTPRedirect("index.html")

    def split_name(self, f):
        m = re.search(".*-(\d+)-(\d+)\.zip", f)
        return (int(m.group(1)), int(m.group(2)))

    @cherrypy.expose
    def download(self, **params):
        id = int(params.get("id", "0").strip())
        if id == 0:
            pass
        xname = str(id) + ".fb2"
        tname = join(tmp_path, xname)
        onlyfiles = [f for f in os.listdir(mirror_path) if isfile(join(mirror_path, f))]
        for f in onlyfiles:
            if f.endswith(".zip") and ".fb2-" in f:
                (x1, x2) = self.split_name(f)
                if x1 <= id and id <= x2:
                    # cherrypy.log("book must be in "+f)
                    with zipfile.ZipFile(join(mirror_path, f)) as z:
                        with z.open(xname) as zf, open(tname, "wb") as f:
                            shutil.copyfileobj(zf, f)
                            return serve_download(tname)

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def search(self, **params):
        limit_author = params.get("author", "").strip()
        limit_title = params.get("title", "").strip()
        limit_count = int(params.get("count", 10))
        limit_wild = int(params.get("wild", 0))

        if not limit_author and not limit_title:
            limit_title = "hobbit"

        s = select(Book)
        if len(limit_author) > 0:
            s = s.where(
                text("MATCH (author) AGAINST (:a IN BOOLEAN MODE)").bindparams(bindparam("a", limit_author)),
            )
        if len(limit_title) > 0:
            s = s.where(
                text("MATCH (title) AGAINST (:t IN BOOLEAN MODE)").bindparams(bindparam("t", limit_title)),
            )
        s = s.order_by(Book.id.desc())
        if limit_count > 0:
            s = s.limit(limit_count)

        data = []
        with Session(self.engine) as session:
            for b in session.execute(s).all():
                b = b[0]
                # cherrypy.log(f"result is {b}")
                a = [int(b.id), str(b.author), str(b.title), int(b.size), str(b.date)]
                data.append(a)

        return {"data": data}


if __name__ == "__main__":
    cherrypy.config.update({"server.socket_port": 8099})
    cherrypy.quickstart(App())
else:
    cherrypy.config.update({"environment": "embedded"})
    application = cherrypy.Application(App(), script_name=None, config=None)

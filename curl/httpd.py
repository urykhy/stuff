#!/usr/bin/python3
# vim:ts=4:sts=4:sw=4:et
# -*- coding: utf-8 -*-

import string
import os
import time
import cherrypy
from cherrypy.lib import auth_basic
from os.path import isfile, join

USERS = {'name': 'secret'}

def validate_password(realm, username, password):
    if username in USERS and USERS[username] == password:
        return True
    return False

conf = {
   'global' : {
       'server.socket_port': 8088,
    },
   '/auth': {
       'tools.auth_basic.on': True,
       'tools.auth_basic.realm': 'localhost',
       'tools.auth_basic.checkpassword': validate_password,
    },
}

class App(object):
    def __init__(self):
        pass

    @cherrypy.expose
    def hello(self):
        return "Hello World!"

    @cherrypy.expose
    def useragent(self):
        return cherrypy.request.headers.get('User-Agent')

    @cherrypy.expose
    def header(self):
        return cherrypy.request.headers.get('XFF')

    @cherrypy.expose
    def cookie(self):
        return cherrypy.request.cookie['name1'].value

    @cherrypy.expose
    def auth(self):
        return "OK"

    @cherrypy.expose
    def slow(self):
        time.sleep(5)
        return "OK"

    @cherrypy.expose
    def post_handler(self, W):
        return "post: " + W

    @cherrypy.expose
    def method_handler(self, *vpath, **params):
        if "DELETE" == cherrypy.request.method:
            return "OK"
        elif "PUT" == cherrypy.request.method:
            return "PUT: {}".format(cherrypy.request.body.read().decode("utf-8"))
        else:
            return "ERR"

    @cherrypy.expose
    def auto_index(self):
        cherrypy.response.headers['Last-Modified'] = os.path.getmtime(os.path.realpath(__file__))
        cherrypy.lib.cptools.validate_since()
        return """<html>
<head><title>Index of /auto_index/</title></head>
<body bgcolor="white">
<h1>Index of /auto_index/</h1><hr><pre><a href="../">../</a>
<a href="20200331">20200331</a> 01-Apr-2020 10:10           123
<a href="20200401">20200401</a> 01-Apr-2020 10:10           234
</pre><hr></body>
</html>"""

if __name__ == '__main__':
    cherrypy.quickstart(App(), '/', conf)

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
   '/auth': {
       'tools.auth_basic.on': True,
       'tools.auth_basic.realm': 'localhost',
       'tools.auth_basic.checkpassword': validate_password,
    }
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


if __name__ == '__main__':
    cherrypy.quickstart(App(), '/', conf)
else:
    cherrypy.config.update({'environment': 'embedded'})
    application = cherrypy.Application(App(), script_name=None, config=None)


#!/usr/bin/env python

import yaml
import re
from toposort import toposort, toposort_flatten

def makeDict(s):
    return dict(re.findall(r'(\S+)=(".*?"|\w+)', s))

def get_type(type):
    if ',' in type:
        x = makeDict(type)
        return x['type'];
    else:
        return type

def create(name, type):
    if ',' in type:
        x = makeDict(type)
        return x['type'] + ' ' + name + '{' + x['type'] + "::" + x['flags'] + '}'
    else:
        return type + ' ' + name;

def order(doc):
    td = {}
    for x in doc:
        inner = set()
        for e in doc[x]:
            for t in e:
                inner.add(get_type(e[t]))
        td[x] = inner
    td = list(toposort(td))
    ord=[]
    for x in td:
        for y in x:
            if y in doc:
                ord.append(y)
    return ord

def process(doc):
    print "#pragma once"
    print "// auto generated"
    print "namespace Stat {"
    for x in order(doc):
        print "struct "+x+"{"
        for e in doc[x]:
            for t in e:
                print create(t, e[t])+';'
        print "void format(std::ostream& os, const std::string& prefix) {"
        for e in doc[x]:
            for t in e:
                print t+".format(os, prefix+'.'+\""+t+"\");"
        print "}"
        print "};"
    print "} // namespace Stat"

with open("tree.yml", 'r') as stream:
    try:
        process(yaml.load(stream))
    except yaml.YAMLError as exc:
        print(exc)


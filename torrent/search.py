#!/usr/bin/env python3

#
# Licensed under terms of JSON license
# http://www.json.org/license.html
# (c) urykhy
#

from elasticsearch import Elasticsearch
from elasticsearch_dsl import Search, Q

ela_host="localhost"
ela_index="tracker-index"
es = Elasticsearch([ela_host])

limit_cat=""
limit_forum=""
limit_count=10
limit_size_min = 0
limit_size_max = 0

def bytes2human(a):
    if a > 10*1024*1024*1024:
        return str(int(a/(1024*1024*1024)))+"GB"
    elif a > 10*1024*1024:
        return str(int(a/(1024*1024)))+"MB"
    elif a > 10*1024:
        return str(int(a/1024))+"KB"

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

def search(arg):
    s = Search(using=es, index=ela_index)
    if len(limit_cat):
        print ("cat: "+limit_cat+'; ', end="")
        s = s.filter("term", category=limit_cat.split(' '))
    if len(limit_forum):
        print ("forum: "+limit_forum+'; ', end="")
        s = s.filter("term", forum=limit_forum.split(' '))
    if limit_size_min:
        print ("min size: "+limit_size_min+'; ', end="")
        s = s.filter("range", size = {'gte' : limit_size_min })
    if limit_size_max:
        print ("max size: "+limit_size_max+'; ', end="")
        s = s.filter("range", size = {'lte' : limit_size_max })
    arg = arg.split(' ')
    q = Q("match", name=arg.pop(0))
    for a in arg:
        q = q & Q("match", name=a)
    s = s.query(q)

    body = s.to_dict()
    print (body)

    r = s.execute()
    print ('total hits:',r.hits.total)
    size = r.hits.total
    if size > limit_count:
        size = limit_count

    #s = s.sort('name.raw')
    s = s.sort('-size')
    s = s.extra(size=size)
    r = s.execute()

    print ('printing:',len(r),"first ...")
    #for hit in sorted(r, key=lambda k: k._body.name.lower()):
    for hit in r:
        b = hit._body
        print("%8s %7s %s" % (b.id, bytes2human(b.size), b.name+'/cat:'+b.category+'/forum:'+b.forum))

def get(arg):
    s = Search(using=es, index=ela_index) \
            .query("match", id=arg)
    r = s.execute()
    b = r[0]['_body']
    print (' name:',b.name)
    print (' hash:',b.hash)
    print (' size:',b.size)
    print (' date:',b.date[0] if b.date else '')
    print (' cat:',b.category)
    print (' forum:',b.forum)

if __name__ == '__main__':
    import cmd
    class Shell(cmd.Cmd):
        intro = 'Welcome to the shell. Type help in doubt.\n'
        prompt = '$ '
        def do_s(self, arg):
            search(arg)
        def do_g(self, arg):
            get(arg)
        def do_cat(self, arg):
            global limit_cat
            limit_cat = arg.lower()
        def do_forum(self, arg):
            global limit_forum
            limit_forum = arg.lower()
        def do_count(self, arg):
            global limit_count
            limit_count = int(arg)
        def do_size(self, arg):
            global limit_size_min
            global limit_size_max
            [limit_size_min, limit_size_max] = map(human2bytes ,arg.split(' '))
        def do_nolimit(self, arg):
            global limit_cat
            global limit_forum
            global limit_count
            global limit_size_min
            global limit_size_max
            limit_cat = ""
            limit_forum = ""
            limit_count = 10
            limit_size_min = 0
            limit_size_max = 0
        def do_help(self, arg):
            print('''
Well, we have 2 main commands here: s(for search) and g(for get)
s can take some arguments to perform full-text search over elasticSearch,
and g <id> can be used to get detailed information (ie hash)

few commands can be used to narrow search results:
cat <name> -- search only in specified category
forum <name> -- search only in specified forum
count <num> -- change number of search results to show at most
size <min> <max> -- set torrent size limits, human friendly, ie 10K and 20M works
nolimit -- revert limits to default.

use quit or q to exit.
            ''')
        def do_quit(self,arg):
            return True
        def do_q(self,arg):
            return True
    Shell().cmdloop()


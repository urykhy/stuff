#!/usr/bin/env python

#
# Licensed under terms of JSON license
# http://www.json.org/license.html
# (c) urykhy
#

import os, sys, string, codecs
import zipfile,shutil

mirror_path = "/u02/mirror/fb2.Flibusta.Net"
tmp_path = "/tmp"
mlist = {}

# TODO: support regexps ?

def read_inp(z,fname):
    print "read",fname
    llist = []
    with z.open(fname) as f:
        for l in f:
            l=l.strip().decode(sys.stdin.encoding)
            (au, genre, name, seq, _None, id, _None) = l.split("\04",6)
            if len(seq):
                llist.append((au, name + '/' + seq, id))
            else:
                llist.append((au, name, id))
            #print au,"\n",genre,"\n",name,"\n",id
    mlist[os.path.basename(fname)] = llist

with zipfile.ZipFile(mirror_path + "/flibusta_fb2_local.inpx") as zfile:
    for info in zfile.infolist():
        if info.filename.endswith('.inp'):
            read_inp(zfile, info.filename)

print "readed index files: "
for i in mlist:
    print i,
print "\nready ..."

def search_author(st):
    for p in mlist:
        for (au, name, id) in mlist[p]:
            if st in au.lower():
                print id,au,name
def search_book(st):
    for p in mlist:
        for (au, name, id) in mlist[p]:
            if st in name.lower():
                print id,au,name
def get_book(st):
    for p in mlist:
        for (au, name, id) in mlist[p]:
            if st == id:
                p = p.replace("inp","zip")
                p = mirror_path + "/" + p
                print "found in",p
                ftx = st + ".fb2"
                fx = os.path.join(tmp_path,ftx)
                with zipfile.ZipFile(p) as z:
                    with z.open(ftx) as zf, open(fx, 'wb') as f:
                        shutil.copyfileobj(zf, f)
                print "saved as",fx
                return

import readline
readline.parse_and_bind('tab: complete')
readline.parse_and_bind('set editing-mode vi')

while True:
    line = raw_input('Enter command ("quit" to quit): ').decode(sys.stdin.encoding)
    if line == 'quit' or line == 'q' :
        break
    (cmd,arg) = line.split(" ",1)
    arg = arg.lower()
    if cmd == "a":
        search_author(arg)
    if cmd == "b":
        search_book(arg)
    if cmd == "g":
        get_book(arg)


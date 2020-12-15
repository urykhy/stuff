#!/usr/bin/env python3

from stat import *
import os
import shutil
import subprocess
import tarfile
import tempfile
import yaml

import logging
logging.basicConfig(format='%(asctime)s %(levelname)s: %(message)s', level=logging.DEBUG)

def split_folders(path):
    folders = []
    folders.append(path)
    while 1:
        path, folder = os.path.split(path)
        if folder != "" and path != "":
            folders.append(path)
        else:
            break
    folders.reverse()
    return folders

def write_optional(name, file, cfg):
    if name in c:
        f.write("{}: {}\n".format(name, cfg[name]))

class XTar:
    tar = None
    root = ""
    user = "root"
    group = "root"
    dir_history = {}

    def add_x_dir(self, name):
        name = "." + name
        if name in self.dir_history:
            return
        t = tarfile.TarInfo(name)
        t.type = tarfile.DIRTYPE
        t.uname = self.user
        t.gname = self.group
        t.mtime = os.path.getmtime(".")
        t.mode = 0o755
        self.tar.addfile(t)
        self.dir_history[name] = 1

    def add_root(self):
        for x in split_folders(self.root):
            self.add_x_dir(x)

    def add_dir(self, name):
        for x in split_folders(name):
            mtar.add_x_dir(os.path.join(self.root, x))

    # TODO: add recursion
    def add_file(self, name, folder):
        tname = "." + os.path.join(self.root, folder, os.path.basename(name))
        t = tarfile.TarInfo(tname)
        t.type = tarfile.REGTYPE
        t.uname = self.user
        t.gname = self.group
        t.size = os.path.getsize(name)
        t.mtime = os.path.getmtime(name)
        if os.access(name, os.X_OK):
            t.mode = 0o755
        else:
            t.mode = 0o644
        self.tar.addfile(t, open(name, 'rb'))

    def __init__(self, tar, c):
        self.tar = tar
        self.root = c["root"] if "root" in c else ""
        self.user = c["user"]
        self.group = c["group"]
        self.add_root()

with open("package.yml", 'r') as f:
    try:
        cfg = yaml.load(f)
    except yaml.YAMLError as exc:
        print(exc)

dirpath = tempfile.mkdtemp()
logging.info("using build directory {}".format(dirpath))

for c in cfg:
    root = c["root"] if "root" in c else ""

    logging.info("build package {}".format(c["package"]))
    logging.debug("... control")
    with open(os.path.join(dirpath,"control"), "w") as f:
        for opt in ("package","version","architecture","maintainer","homepage"):
            f.write("{}: {}\n".format(opt, c[opt]))
        for opt in ("depends","pre-depends","recommends","suggests","breaks","conflicts","replaces","provides","section","priority","tag","bugs","source"):
            write_optional(opt, f, c)
        f.write("description: {}\n".format(c["short"]))
        f.write("  {}\n".format(c["long"]))

    logging.debug("... conffiles")
    with open(os.path.join(dirpath,"conffiles"), "w") as f:
        for dir in c["files"]:
            for cf in dir:
                if cf == "etc":
                    for e in dir[cf]:
                        f.write("{}\n".format(os.path.join(root, cf, os.path.basename(e))))

    logging.debug("... data.tar.gz")
    with tarfile.open(os.path.join(dirpath,"data.tar.gz"), "w:gz") as tar:
        mtar = XTar(tar, c)
        for dir in c["files"]:
            for cf in dir:
                mtar.add_dir(cf)
                for e in dir[cf]:
                    mtar.add_file(e, cf)

    logging.debug("... debian-binary")
    with open(os.path.join(dirpath,"debian-binary"), "w") as f:
        f.write("2.0\n")

    logging.debug("... control.tar.gz")
    with tarfile.open(os.path.join(dirpath,"control.tar.gz"), "w:gz") as tar:
        tar.add(os.path.join(dirpath,"control"), "control")
        tar.add(os.path.join(dirpath,"conffiles"), "conffiles")

    logging.debug("... collect to deb")
    pname = c["package"] + "-" + str(c["version"]) + ".deb"
    subprocess.call(["ar", "-qS", pname, os.path.join(dirpath,"debian-binary"), os.path.join(dirpath,"control.tar.gz"), os.path.join(dirpath,"data.tar.gz")])

    logging.debug("... done!")
logging.info("cleanup")
shutil.rmtree(dirpath)

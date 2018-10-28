#!/usr/bin/env python

import subprocess
import tarfile
import os
import tempfile
import shutil
import yaml
import logging
logging.basicConfig(format='%(asctime)s %(levelname)s: %(message)s', level=logging.DEBUG)

def tar_add_dir(file, name, user, group):
    t = tarfile.TarInfo(name)
    t.type = tarfile.DIRTYPE
    t.uname = user
    t.gname = group
    file.addfile(t)

def tar_add_root(file, path, user, group):
    folders = []
    folders.append(path)
    while 1:
        path, folder = os.path.split(path)
        if folder != "":
            folders.append(path)
        else:
            break
    folders.reverse()
    for x in folders:
        tar_add_dir(file, "." + x, user, group)

def tar_file(file, name, user, group):
    t = tarfile.TarInfo(name)
    t.type = tarfile.REGTYPE
    t.uname = user
    t.gname = group
    t.size = os.path.getsize(file)
    return t

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
        f.write("Package: {}\n".format(c["package"]))
        f.write("Version: {}\n".format(c["version"]))
        f.write("Architecture: {}\n".format(c["architecture"]))
        f.write("Maintainer: {}\n".format(c["maintainer"]))
        f.write("Description: {}\n".format(c["short"]))
        f.write("  {}\n".format(c["long"]))

    logging.debug("... conffiles")
    with open(os.path.join(dirpath,"conffiles"), "w") as f:
        for x in c["etc"]:
            f.write("{}\n".format(os.path.join(root, "etc", x)))

    logging.debug("... data.tar.gz")
    with tarfile.open(os.path.join(dirpath,"data.tar.gz"), "w:gz") as tar:
        tar_add_root(tar, root, c["user"], c["group"])
        if "etc" in c:
            tar_add_dir(tar, "." + os.path.join(root, "etc"), c["user"], c["group"])
            for x in c["etc"]:
                tar.addfile(tar_file(x, "." + os.path.join(root, "etc", x), c["user"], c["group"]), open(x))
        if "init" in c:
            tar_add_dir(tar, "." + os.path.join(root, "etc/init.d"), c["user"], c["group"])
            for x in c["init"]:
                tar.addfile(tar_file(x, "." + os.path.join(root, "etc/init.d", x), c["user"], c["group"]), open(x))
        if "bin" in c:
            tar_add_dir(tar, "." + os.path.join(root, "bin"), c["user"], c["group"])
            for x in c["bin"]:
                tar.addfile(tar_file(x, "." + os.path.join(root, "bin", x), c["user"], c["group"]), open(x))

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

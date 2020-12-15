#!/usr/bin/env python

# feel free to use,reuse and abuse code in this file

import os.path
import os
import time
import time as time

# ctest (project must be configured with gcov)
# lcov --directory . --capture --output-file info.dirty;
# lcov --remove info.dirty 'tests/*' '/usr/*' --output-file info.cleaned
# cat info.cleaned | c++filt > info.cxx
# ./coverage-allure.py
# mv coverage*.xml /tmp/allure/
# allure generate -o /tmp/allure /tmp/allure;
# firefox /tmp/allure/index.html

f = open("info.cxx")
test_start_time = int(os.path.getmtime("info.cxx") * 1000) # in ms

import xml.dom.minidom as minidom
from xml.etree.ElementTree import Element, SubElement, tostring
root = Element('ns0:test-suite')
root.set("xmlns:ns0", "urn:model.allure.qatools.yandex.ru")
root.set("start", str(test_start_time))
root.set("stop", str(test_start_time + 1000))   # get time used to perform prep. steps
SubElement(root, "name").text = "Coverage Test"
cases = SubElement(root, "test-cases")

cwd = os.getcwd()
flist_ok = []
flist_bad = []
fname = ""
for i in f.readlines():
    i = i.strip()
    if i.startswith("SF:"):
        fname = i[3:]
        if fname.startswith(cwd):
            fname = fname[len(cwd) + 1:]
        print fname
    elif i.startswith("FNDA:"):
        (number,name) = i[5:].split(',',1)
        if int(number) > 0:
            flist_ok.append(name)
        else:
            flist_bad.append(name)
    elif i == "end_of_record":
        flist_ok = sorted(set(flist_ok))
        flist_bad_old = sorted(set(flist_bad))
        flist_bad = []
        for el in flist_bad_old:
            if el not in flist_ok:
                flist_bad.append(el)

        # write xml here
        e = SubElement(cases,"test-case")
        e.set("start", str(test_start_time))
        e.set("stop", str(test_start_time + 1))
        if len(flist_bad) == 0:
            e.set("status", "passed")
        elif len(flist_bad) < len(flist_ok):
            e.set("status", "broken")
        else:
            e.set("status", "failed")
        SubElement(e, "name").text = fname
        SubElement(e, "title").text = fname
        steps = SubElement(e, "steps")
        for e in flist_ok:
            ei = SubElement(steps, "step")
            ei.set("start", str(test_start_time))
            ei.set("stop", str(test_start_time))
            ei.set("status", "passed")
            SubElement(ei, "title").text = e
            SubElement(ei, "name").text = e
        for e in flist_bad:
            ei = SubElement(steps, "step")
            ei.set("start", str(test_start_time))
            ei.set("stop", str(test_start_time))
            ei.set("status", "failed")
            SubElement(ei, "title").text = e
            SubElement(ei, "name").text = e

        # clear
        flist_ok = []
        flist_bad = []
        fname = ""

print >>open("coverage-"+str(time.time())+"-testsuite.xml","w"), minidom.parseString(tostring(root)).toprettyxml(indent="\t")


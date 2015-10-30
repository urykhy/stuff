#!/usr/bin/env python

# feel free to use,reuse and abuse code in this file

import os.path, time
import re
import time as time

# mkdir /tmp/allure/
# ctest > ctest.out
# ./ctest-allure.py
# mv ctest*.xml /tmp/allure/
# allure generate -o /tmp/allure /tmp/allure;
# firefox /tmp/allure/index.html

f = open("ctest.out")
test_start_time = int(os.path.getmtime("ctest.out") * 1000) # in ms

import xml.dom.minidom as minidom
from xml.etree.ElementTree import Element, SubElement, tostring
root = Element('ns0:test-suite')
root.set("xmlns:ns0", "urn:model.allure.qatools.yandex.ru")
root.set("start", str(test_start_time))
SubElement(root, "name").text = "ctest"
cases = SubElement(root, "test-cases")

prog = re.compile(".*Test\W+#(\d+): ([^ ]+)\W+.+(Passed|Failed)\W+(\d+.\d+) sec")
total = re.compile("^Total Test time.*=\W+(\d+.\d+)")
for i in f.readlines():
    m = total.match(i)
    if m:
        root.set("stop", str(test_start_time + int(float(m.group(1)) * 1000)))
    m = prog.match(i)
    if m:
        print m.group(1),m.group(2),m.group(3),m.group(4)
        e = SubElement(cases,"test-case")
        e.set("start", str(test_start_time))
        e.set("stop", str(test_start_time + int(float(m.group(4)) * 1000)))
        e.set("status", m.group(3).lower())
        SubElement(e, "name").text = m.group(2)
        SubElement(e,"attachments")
        labels = SubElement(e,"labels")
        #l = SubElement(labels, "label")
        #l.set("name", "story")
        #l.set("value", m.group(2))
        #l = SubElement(labels, "label")
        #l.set("name", "feature")
        #l.set("value", m.group(2))
        SubElement(e,"steps")
        if m.group(3) == "Failed":
            f = SubElement(e,"failure")
            msg = ""
            bt = ""
            start_time = 0
            test_start_re=re.compile("^\""+m.group(2)+"\" start time")
            with open("Testing/Temporary/LastTest.log") as xf:
                for l in xf.readlines():
                    if start_time == 2:
                        if l.strip() == "<end of output>":
                            break
                        if l.strip().startswith("tests summary"):
                            msg = l.strip()
                            break
                        if l.strip().startswith("*** "):
                            msg = l.strip()
                            break
                        bt += l
                    elif start_time == 1 and l.strip( )== "----------------------------------------------------------":
                        start_time = 2
                    elif start_time == 0 and test_start_re.match(l):
                        start_time = 1
            SubElement(f,"message").text = msg
            SubElement(f,"stack-trace").text = bt

print >>open("ctest-"+str(time.time())+"-testsuite.xml","w"), minidom.parseString(tostring(root)).toprettyxml(indent="\t")


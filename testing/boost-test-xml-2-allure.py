#!/usr/bin/env python3
# vim:ts=4:sts=4:sw=4:et
# -*- coding: utf-8 -*-

# feel free to use,reuse and abuse code in this file

# mkdir /tmp/allure
# run test: ./a.out -f XML -l all > test.xml
# process : ./boost-test-xml-2-allure.py test.xml
# upload  : for x in ctest*.xml; do ~/code/docker/allure/upload.sh $x; done
#
# https://github.com/allure-framework/allure1/blob/master/allure-model/src/main/resources/allure.xsd

import collections
import itertools
import os.path
import sys
import time
import time as time
from xml.etree.ElementTree import parse, Element, SubElement, tostring
import xml.dom.minidom as minidom


def process_case(tcase):
    global sum_ela

    case_name = tcase.attrib["name"]
    print(f"handle case {case_name}")
    ela = int(tcase.find("TestingTime").text) / 1000 + 1
    sum_ela += ela

    e = SubElement(cases, "test-case")
    e.set("start", str(test_start_time))
    e.set("stop", str(test_start_time + int(ela)))
    SubElement(e, "name").text = case_name
    SubElement(e, "attachments")
    labels = SubElement(e, "labels")
    steps = SubElement(e, "steps")
    isFailed = 0
    err_msgs = []

    for tstep in tcase.iter('Info'):
        ei = SubElement(steps, "step")
        ei.set("start", str(test_start_time))
        ei.set("stop", str(test_start_time + int(ela)))
        ei.set("status", "passed")
        SubElement(ei, "title").text = tstep.text

    for tstep in itertools.chain(tcase.iter('Error'), tcase.iter('Exception')):
        ei = SubElement(steps, "step")
        ei.set("start", str(test_start_time))
        ei.set("stop", str(test_start_time + int(ela)))
        ei.set("status", "failed")
        SubElement(ei, "title").text = tstep.text
        code_line = tstep.attrib["file"] + ":" + tstep.attrib["line"]
        isFailed += 1
        err_msgs.append(code_line)

    if isFailed > 0:
        e.set("status", "failed")
        f = SubElement(e, "failure")
        SubElement(f, "message").text = suite_name + " have " + str(isFailed) + " failed checks"
        SubElement(f, "stack-trace").text = '\n'.join(err_msgs)
    else:
        e.set("status", "passed")


for name in sys.argv[1:]:
    print(f"open file {name}")
    doc = parse(name)
    test_start_time = int(os.path.getctime(name) * 1000)  # in ms

    for ts in doc.iter("TestSuite"):
        if len(ts.findall('TestCase')) == 0:
            continue
        suite_name = ts.attrib["name"]
        parent = doc.find('.//TestSuite[@name="' + suite_name + '"]...').attrib["name"]
        if parent != "Suites":
            suite_name = parent + "." + suite_name

        print(f"handle suite {suite_name}")
        root = Element('ns0:test-suite')
        root.set("xmlns:ns0", "urn:model.allure.qatools.yandex.ru")
        root.set("start", str(test_start_time))
        SubElement(root, "name").text = suite_name
        cases = SubElement(root, "test-cases")
        sum_ela = 0
        for tcase in ts.findall('TestCase'):
            process_case(tcase)
        root.set("stop", str(test_start_time + int(sum_ela)))
        print(minidom.parseString(tostring(root)).toprettyxml(indent="\t"),
              file=open("ctest-" + suite_name + "-testsuite.xml", "w"))

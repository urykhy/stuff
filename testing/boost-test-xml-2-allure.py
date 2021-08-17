#!/usr/bin/env python3
# vim:ts=4:sts=4:sw=4:et
# -*- coding: utf-8 -*-

# feel free to use,reuse and abuse code in this file

# run test: ./a.out -f XML -l all -k test.xml
# process : ./boost-test-xml-2-allure.py test.xml
# upload  : for x in ctest*.xml; do ~/code/docker/allure/upload.sh $x; done
#
# https://github.com/allure-framework/allure1/blob/master/allure-model/src/main/resources/allure.xsd

import re
import sys
import os
import yaml
import pathlib
from jinja2 import Template, Environment, FileSystemLoader
from xml.etree.ElementTree import parse

split_line = '--split--ZYuMROhO04II4h2elr6f7DryY7N1y3--'

meta = {
    'time': int(os.path.getctime(sys.argv[1]) * 1000) # in ms
}

script_path = str(pathlib.Path(__file__).parent.absolute())
environment = Environment(extensions=['jinja2.ext.do'], loader=FileSystemLoader(searchpath=script_path))
template = environment.from_string(open(script_path + '/boost-test.j2').read())
doc = parse(sys.argv[1])
extra = yaml.safe_load(open(script_path + '/boost-test.yaml', 'r'))

current_file = None
for x in template.render(doc=doc, meta=meta, extra=extra).split('\n'):
    if x.find(split_line) > -1:
        current_file = open("ctest-" + x.split(split_line)[1] + "-testsuite.xml", "w")
    else:
        if current_file:
            print (x, file=current_file)
#!/usr/bin/env python3

import base64
import io
import json
import pathlib
import sys
import zlib

from jinja2 import Template, Environment, FileSystemLoader
from urllib.parse import unquote
from xml.etree.ElementTree import parse, fromstring

for x in parse(sys.argv[1]).findall("diagram"):
    name = x.attrib["name"]
    # decode with help from https://crashlaker.github.io/programming/2020/05/17/draw.io_decompress_xml_python.html
    #obj = zlib.decompressobj(-15)
    #xml = unquote(obj.decompress(base64.b64decode(x.text)) + obj.flush())
    #print(xml)
    #doc = fromstring(xml)

    doc = x
    script_path = str(pathlib.Path(__file__).parent.absolute())
    environment = Environment(
        extensions=["jinja2.ext.do"], loader=FileSystemLoader(searchpath=script_path)
    )
    template = environment.from_string(open(script_path + "/generator.j2").read())
    doc.attrib["name"] = name
    print(template.render(doc=doc))

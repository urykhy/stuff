#!/usr/bin/env python3

import json
import pathlib
import re
import sys

import lark
from jinja2 import Environment, FileSystemLoader, Template

parser = lark.Lark(
    r"""
    start         : (namespace | class | link)+

    namespace     : "namespace" NAMESPACENAME "{" (class)+ "}"
    class         : "class" CLASSNAME "{" (member|method)+ "}"
    member        : TYPENAME VALNAME
    method        : "+" METHODNAME "(" ARGNAME? ")" RETURNTYPE?
    link          : REL_FROM "..>" REL_TO comment?
    comment       : ":" STRING+

    NAMESPACENAME : STRING
    CLASSNAME     : STRING
    VALNAME       : STRING
    METHODNAME    : STRING
    TYPENAME      : STRING
    RETURNTYPE    : STRING
    ARGNAME       : STRING
    REL_FROM      : STRING
    REL_TO        : STRING

    DEFAULT       : /[a-zA-Z0-9._-]+/
    %import common.CNAME -> STRING
    %import common.WS
    %ignore WS
"""
)

s = open(sys.argv[1], "r").read()
s = re.findall("```mermaid\\nclassDiagram\\n([^\`]+)```", s)[0]

x = parser.parse(s)


def convert(x):
    doc = {}

    def step(i):
        items = []
        item = {}
        for i in i.children:
            if type(i) is lark.Tree:
                items.append({str(i.data): step(i)})
            if type(i) is lark.Token:
                item[i.type] = i.value
        if len(item) > 0 and len(items) > 0:
            item["contains"] = items
            return item
        if len(items) == 0:
            return item
        return items

    doc["root"] = step(x)
    return doc


doc = convert(x)
# print(json.dumps(doc, indent=4))

script_path = str(pathlib.Path(__file__).parent.absolute())
environment = Environment()
dto = json.load(open(script_path + "/dto.json"))

template = environment.from_string(open(script_path + "/generator.j2").read())
print(template.render(doc=doc, dto=dto))

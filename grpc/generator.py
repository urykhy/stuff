#!/usr/bin/env python

import json
import pathlib
import sys

from jinja2 import Environment, FileSystemLoader, Template
from lark import Discard, Lark, Transformer

grammar = r"""
start: (syntax | package | message | service)*

syntax: "syntax" "=" ESCAPED_STRING ";"
package: "package" IDENTIFIER ";"

message: "message" IDENTIFIER "{" intm* "}"
intm: field | message
field: option? TYPE IDENTIFIER "=" NUMBER ";"
option: "repeated" | "optional" | "required"

service: "service" IDENTIFIER "{" method* "}"
method: "rpc" IDENTIFIER arg "returns" arg [ "{}" | ";"]
arg: "(" TYPE ")"

NUMBER: /[0-9]+/
IDENTIFIER: /[a-zA-Z_][a-zA-Z0-9_]*/
TYPE: IDENTIFIER

%import common.ESCAPED_STRING
%import common.WS
%ignore WS

COMMENT: "/*" /(.|\n)*?/ "*/"
       | "//" /[^\n]*/
%ignore COMMENT
"""
parser = Lark(grammar)
tree = parser.parse(open(sys.argv[1], "r").read())


class ProtoTransformer(Transformer):
    def start(self, items):
        return {"proto": items}

    def syntax(self, items):
        return Discard

    def package(self, items):
        return {"package": str(items[0])}

    def message(self, items):
        return Discard

    def type(self, items):
        return items[0]

    def service(self, items):
        return {"service": str(items[0]), "methods": items[1:]}

    def method(self, items):
        return {"method": str(items[0]), "request": items[1], "response": items[2]}

    def arg(self, items):
        return {"type": str(items[0])}


doc = ProtoTransformer().transform(tree)["proto"]
# print(f"{json.dumps(doc, indent=4)}")

script_path = str(pathlib.Path(__file__).parent.absolute())
environment = Environment(extensions=["jinja2.ext.do"], loader=FileSystemLoader(searchpath=script_path))
template = environment.from_string(open(script_path + "/grpc.j2").read())
# print(template.render(doc=doc))

with open(sys.argv[2], "w") as f:
    print(template.render(doc=doc), file=f)

#!/usr/bin/env python3

import pathlib
import re
import subprocess
import sys

import lark
from jinja2 import Environment, FileSystemLoader, Template


class AttrDict(dict):
    def __init__(self, *args, **kwargs):
        super(AttrDict, self).__init__(*args, **kwargs)
        self.__dict__ = self


parser = lark.Lark(
    r"""
    start        : cmd+
    cmd          : "syntax" "=" "\"" STRING "\"" SEMICOLON -> syntax
                 | "package" xname SEMICOLON               -> package
                 | "option" name "=" STRING SEMICOLON
                 | message -> message
                 | enum    -> enum

    message      : "message" name "{" (entry|enum|message|comment)+ "}" SEMICOLON?
    entry        : kind type name "=" id packed? default? SEMICOLON comment?
    enum         : "enum" name "{" enum_entry+ "}"
    enum_entry   : name "=" value SEMICOLON

    default      : "[" "default" "=" DEFAULT "]"
    packed       : "[" "packed" "=" STRING "]"
    kind         : REQUIRED|OPTIONAL|REPEATED
    type         : STRING
    name         : STRING
    xname        : DEFAULT
    id           : NUMBER
    value        : NUMBER
    comment      : /\/\/.*/

    SEMICOLON    : ";"
    REQUIRED     : "required"
    OPTIONAL     : "optional"
    REPEATED     : "repeated"
    DEFAULT      : /[a-zA-Z0-9._-]+/
    %import common.CNAME            -> STRING
    %import common.SIGNED_NUMBER    -> NUMBER
    %import common.WS
    %ignore WS
"""
)

x = parser.parse(open(sys.argv[1], "r").read())
# print('/*')
# print(x.pretty())
# print('*/')


def convert(x):
    doc = {}
    localTypeNames = []
    localEnums = []

    def get_by_name(name, x):
        for i in x.children:
            if i == ";":
                continue
            if name == i.data:
                return i.children[0]
        return None

    def makeEntry(i):
        def _customType(proto_type):
            return proto_type in localTypeNames

        def _encoding(name):
            xtype = {
                "fixed32": "Protobuf::FIXED",
                "sint32": "Protobuf::ZIGZAG",
                "fixed64": "Protobuf::FIXED",
                "sint64": "Protobuf::ZIGZAG",
            }
            if name in xtype:
                return ", " + xtype[name]
            return ""

        def _type(string_view, name):
            xtype = {
                "string": "std::pmr::string",
                "bytes": "std::pmr::string",
                "int32": "int32_t",
                "sint32": "int32_t",
                "uint32": "uint32_t",
                "fixed32": "uint32_t",
                "int64": "int64_t",
                "sint64": "int64_t",
                "uint64": "uint64_t",
                "fixed64": "uint64_t",
            }
            if string_view and name in ("string", "bytes"):
                return "std::string_view"
            if name in xtype:
                return xtype[name]
            return name

        x = AttrDict()
        x.id = get_by_name("id", i)
        x.name = get_by_name("name", i)
        x.value = get_by_name("value", i)
        x.default = get_by_name("default", i)
        x.repeated = get_by_name("kind", i) == "repeated"
        x.packed = get_by_name("packed", i) == "true"
        x.proto_type = get_by_name("type", i)

        string = x.proto_type == "string" or x.proto_type == "bytes"
        string_view = "use:string_view" in (get_by_name("comment", i) or "")
        x.pmr = (
            True if (string and not string_view) or _customType(x.proto_type) else False
        )

        x.cxx_type = _type(string_view, x.proto_type)
        x.custom_type = _customType(x.proto_type)
        x.encoding = _encoding(x.proto_type)

        x.enum = x.proto_type in localEnums
        x.cast = "sTmp" if not x.enum else "static_cast<" + x.cxx_type + ">(sTmp)"
        return x

    def step_enum(i):
        localEnums.append(get_by_name("name", i))
        t = {}
        t["name"] = get_by_name("name", i)
        for x in i.children[1:]:
            t.setdefault("fields", []).append(makeEntry(x))
        return t

    def step_message(i):
        t = {}
        for x in i:
            if isinstance(x, lark.lexer.Token) and x.type == 'SEMICOLON':
                continue
            if x.data == "name":
                t["name"] = x.children[0]
                localTypeNames.append(x.children[0])
            elif x.data == "entry":
                t.setdefault("fields", []).append(makeEntry(x))
            elif x.data == "enum":
                t.setdefault("enum", []).append(step_enum(x))
            elif x.data == "message":
                t.setdefault("message", []).append(step_message(x.children))
        return t

    def step(i):
        if i.data == "package":
            doc["namespace"] = "pmr_" + get_by_name("xname", i)
        if i.data == "message":
            doc.setdefault("message", []).append(step_message(i.children[0].children))

    for i in x.children:
        step(i)
    return doc


doc = convert(x)
# print('/* converted')
# print(json.dumps(doc, indent=4, sort_keys=True))
# print('*/')

script_path = str(pathlib.Path(__file__).parent.absolute())
environment = Environment(
    extensions=["jinja2.ext.do"], loader=FileSystemLoader(searchpath=script_path)
)


def gperf(input):
    p = subprocess.Popen(
        ["gperf", "-t", "-E"], stdin=subprocess.PIPE, stdout=subprocess.PIPE
    )
    p.stdin.write(input.encode("utf-8"))
    p.stdin.close()
    p.wait()
    s = p.stdout.read().decode("utf-8")
    s = re.compile(r"static unsigned int.*", re.DOTALL).findall(s)[0]
    s = re.sub("register", "", s)
    s = re.sub("struct ReflectionKey \*", "static const ReflectionKey *", s)
    s = re.sub(
        "static struct ReflectionKey wordlist", "static const ReflectionKey wordlist", s
    )
    return s


def proto_gperf(fields):
    input = "%define hash-function-name ReflectionHash\n%define lookup-function-name ReflectionGet\nstruct ReflectionKey {const char* name = nullptr; uint32_t id = 0;};\n%%\n"
    for f in fields:
        input += f"{f.name}, {f.id}\n"
    input += "%%\n"
    return gperf(input)


environment.globals["proto_gperf"] = proto_gperf

template = environment.from_string(open(script_path + "/protobuf.j2").read())
print(template.render(doc=doc))

sys.exit(0)

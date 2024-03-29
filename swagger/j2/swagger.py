#!/usr/bin/env python3

import os
import re
import sys
from pathlib import Path
from urllib.parse import urlparse

import yaml
from jinja2 import Environment, FileSystemLoader

doc = {}


def expand_ref(item):
    if "$ref" in item:
        c = None
        for x in item["$ref"].split("/"):
            if x == "#":
                c = doc
            else:
                c = c[x]
        return c
    else:
        return item


def make_template(name):
    script_path = str(Path(__file__).parent.absolute())
    environment = Environment(
        extensions=["jinja2.ext.do"], loader=FileSystemLoader(searchpath=script_path)
    )

    def swagger_raise(message):
        raise Exception("Jinja2: %s" % (message))

    environment.globals["swagger_raise"] = swagger_raise

    def swagger_prefix(p):
        xr = []
        for n in urlparse(p).path.split("/"):
            if n.find("{") != -1:
                break
            xr.append(n)
        return "/".join(xr)

    environment.filters["swagger_prefix"] = swagger_prefix

    def swagger_expand_ref(p):
        return expand_ref(p)

    environment.filters["swagger_expand_ref"] = swagger_expand_ref

    def swagger_expand_refs(p):
        return list(map(lambda x: expand_ref(x), p))

    environment.filters["swagger_expand_refs"] = swagger_expand_refs

    def swagger_path(p):
        return urlparse(p).path

    environment.filters["swagger_path"] = swagger_path

    def swagger_enum(p):
        e = p["schema"]["enum"]
        xt = p["schema"]["type"]
        if xt == "string":
            return ",".join(list(map(lambda x: f'"{x}"', e)))
        return ",".join(list(map(lambda x: f"{x}", e)))

    environment.filters["swagger_enum"] = swagger_enum

    def swagger_method(x):
        if x == "delete":
            x += "_"
        return x

    environment.filters["swagger_method"] = swagger_method

    def swagger_type(x):
        if x == "string":
            return "std::string"
        if x == "number":
            return "double"
        if x == "integer":
            return "int64_t"
        if x == "boolean":
            return "bool"
        if x.startswith("array<"):
            xt = re.compile("array<(.*)>").search(x).group(1)
            return f"std::vector<{swagger_type(xt)}>"
        return x
        # raise Exception("unexpected type: " + str(x))

    environment.filters["swagger_type"] = swagger_type
    return environment.from_string(open(script_path + name).read())


def render(api_name):
    template = make_template("/swagger.j2")
    global doc
    doc = yaml.safe_load(open(api_name, "r"))

    meta_path = os.path.join(Path(api_name).parent.absolute(), "_meta.yaml")
    if os.path.exists(meta_path):
        meta_doc = yaml.safe_load(open(meta_path, "r"))
        doc["info"].update(meta_doc.get(Path(api_name).name, {}))

    hpp_name = Path(api_name).name.replace(".yaml", ".hpp")
    print(template.render(doc=doc), file=open(hpp_name, "w"))

    doc["info"]["x-hpp-name"] = hpp_name
    cpp_name = Path(api_name).name.replace(".yaml", ".cpp")
    print(template.render(doc=doc), file=open(cpp_name, "w"))


if __name__ == "__main__":
    render(sys.argv[1])

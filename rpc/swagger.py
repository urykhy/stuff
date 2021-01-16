#!/usr/bin/env python3

import os
import pathlib
import sys
import yaml
from jinja2 import Template, Environment
from urllib.parse import urlparse

script_path = str(pathlib.Path(__file__).parent.absolute())
environment = Environment()


def swagger_prefix(p):
    xr = []
    for n in urlparse(p).path.split('/'):
        if n.find('{') != -1:
            break
        xr.append(n)
    return '/'.join(xr)
environment.filters['swagger_prefix'] = swagger_prefix


def swagger_path(p):
    return urlparse(p).path
environment.filters['swagger_path'] = swagger_path


def swagger_method(x):
    if x == "delete":
        x += "_"
    return x
environment.filters['swagger_method'] = swagger_method


def swagger_type(x):
    if x == "string":
        return "std::string"
    if x == "number":
        return "double"
    if x == "integer":
        return "uint64_t"
    raise Exception("unexpected type: " + str(x))
environment.filters['swagger_type'] = swagger_type


def swagger_assign(x, name):
    xt = x['schema']['type']
    if xt == 'string':
        return f"{name}"
    if xt == 'integer':
        return f"Parser::Atoi<uint64_t>({name})";
    if xt == 'number':
        return f"Parser::Atof<double>({name})";
    raise Exception("unexpected type: " + str(x))
environment.filters['swagger_assign'] = swagger_assign


template = environment.from_string(open(script_path + '/swagger.j2').read())
doc = yaml.safe_load(open(sys.argv[1], 'r'))
print(template.render(doc=doc))

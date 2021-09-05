#!/usr/bin/env python3

import os
import pathlib
import re
import sys
from urllib.parse import urlparse

import yaml
from jinja2 import Template, Environment, FileSystemLoader

script_path = str(pathlib.Path(__file__).parent.absolute())
environment = Environment(extensions=['jinja2.ext.do'], loader=FileSystemLoader(searchpath=script_path))


def expand_ref(item):
    if '$ref' in item:
        c = None
        for x in item['$ref'].split('/'):
            if x == '#':
                c = doc
            else:
                c = c[x]
        return c
    else:
        return item


def swagger_raise(message):
    raise Exception('Jinja2: %s' % (message))
environment.globals['swagger_raise'] = swagger_raise


def swagger_prefix(p):
    xr = []
    for n in urlparse(p).path.split('/'):
        if n.find('{') != -1:
            break
        xr.append(n)
    return '/'.join(xr)
environment.filters['swagger_prefix'] = swagger_prefix


def swagger_expand_ref(p):
    return expand_ref(p)
environment.filters['swagger_expand_ref'] = swagger_expand_ref


def swagger_expand_refs(p):
    return list(map(lambda x: expand_ref(x), p))
environment.filters['swagger_expand_refs'] = swagger_expand_refs


def swagger_path(p):
    return urlparse(p).path
environment.filters['swagger_path'] = swagger_path


def swagger_enum(p):
    e = p['schema']['enum']
    xt = p['schema']['type']
    if xt == "string":
        return ','.join(list(map(lambda x: f'"{x}"', e)))
    return ','.join(list(map(lambda x: f'{x}', e)))
environment.filters['swagger_enum'] = swagger_enum


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
        return "int64_t"
    if x == "boolean":
        return "bool"
    if x.startswith('array<'):
        xt = re.compile('array<(.*)>').search(x).group(1)
        return f"std::vector<{swagger_type(xt)}>"
    return x
    # raise Exception("unexpected type: " + str(x))
environment.filters['swagger_type'] = swagger_type


template = environment.from_string(open(script_path + '/swagger.j2').read())
doc = yaml.safe_load(open(sys.argv[1], 'r'))
print(template.render(doc=doc))

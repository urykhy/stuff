#!/usr/bin/env python3

import os
import pathlib
import re
import sys
from urllib.parse import urlparse

import yaml
from jinja2 import Template, Environment

script_path = str(pathlib.Path(__file__).parent.absolute())
environment = Environment()


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


def swagger_is_array(p):
    return p.startswith('array<')
environment.tests['swagger_is_array'] = swagger_is_array


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


resultList = []
decodeStack = []
def collapse():
    global decodeStack
    array = False
    arrayItem = None
    objectItem = []
    xname = None
    while True:
        x = decodeStack.pop()
        # print(f"X: {x}", file=sys.stderr)
        if not isinstance(x, str):
            if array:
                arrayItem = x['type']
            else:
                objectItem.append(x)
            continue
        if x.startswith('end'):
            _, t = x.split(' ')
            if t == 'array':
                array = True
        if x.startswith('begin'):
            _, _, xname = x.split(' ')
            break
    if array:
        info = {'type': f"array<{arrayItem}>", 'name': xname}
        if len(decodeStack) == 0:
            resultList.append(info)
        else:
            decodeStack.append(info)
    else:
        info = {'type': f"{xname}_t", 'name': xname, 'fields': objectItem}
        resultList.append(info)
        decodeStack.append(info)


def swagger_decode_step(x, name=''):
    global decodeStack

    x = expand_ref(x)

    if x.get('type') == 'object':
        decodeStack.append(f'begin object {name}')
        pl = x.get('properties', [])
        for v in pl:
            swagger_decode_step(pl[v], v)
        decodeStack.append('end object')
        collapse()
    elif x.get('type') == 'array':
        decodeStack.append(f'begin array {name}')
        swagger_decode_step(x['items'], name)
        decodeStack.append('end array')
        collapse()
    else:
        decodeStack.append({'type': x['type'], 'name': name})


def swagger_decode(x, name='body'):
    global resultList
    global decodeStack
    if x == {}:
        return []

    swagger_decode_step(x, name)

    # just a single value
    if len(decodeStack) == 1 and len(resultList) == 0:
        x = decodeStack.pop()
        info = {'type': swagger_type(x['type']), 'name': name}
        resultList.append(info)

    res = resultList
    resultList = []
    decodeStack = []
    return res
environment.filters['swagger_decode'] = swagger_decode

template = environment.from_string(open(script_path + '/swagger.j2').read())
doc = yaml.safe_load(open(sys.argv[1], 'r'))
print(template.render(doc=doc))

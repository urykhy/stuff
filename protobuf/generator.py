#!/usr/bin/env python3

import sys
from lark import Lark

parser = Lark(r"""
    start        : cmd+
    cmd          : "syntax" "=" "\"" STRING "\"" SEMICOLON -> syntax
                 | "package" name SEMICOLON                -> package
                 | message -> message
                 | enum    -> enum

    message      : "message" name "{" (entry|enum|message)+ "}"
    entry        : kind type name "=" id default? SEMICOLON comment?
    enum         : "enum" name "{" enum_entry+ "}"
    enum_entry   : name "=" value SEMICOLON

    default      : "[" "default" "=" STRING "]"
    kind         : REQUIRED|OPTIONAL|REPEATED
    type         : STRING
    name         : STRING
    id           : NUMBER
    value        : NUMBER
    comment      : /\/\/.*/

    SEMICOLON    : ";"
    REQUIRED     : "required"
    OPTIONAL     : "optional"
    REPEATED     : "repeated"
    %import common.CNAME            -> STRING
    %import common.SIGNED_NUMBER    -> NUMBER
    %import common.WS
    %ignore WS
""")

with open('tutorial.proto', 'r') as file:
    data = file.read()

x = parser.parse(data)
#print ( x.pretty() )
#print ("-"*120)

def fix_type(name):
    xtype = {'string' : 'std::string',
             'int32'  : 'int32_t',
             'fixed32': 'int32_t',
             'sint32' : 'int32_t',
             'float'  : 'float'
            }
    if name in xtype:
        return xtype[name]
    return name

def get_by_name(name, x):
    for i in x.children:
        if i == ";":
            continue
        if name == i.data:
            return i.children[0]
    return None

def step_enum(i):
    print (f"enum {get_by_name('name', i)} {{")
    for x in i.children[1:]:
        print (f"{get_by_name('name', x)} = {get_by_name('value', x)};")
    print ("};")

def step_message(i):
    for x in i:
        if x.data == "name":
            print (f"struct {x.children[0]} {{")
        elif x.data == "entry":
            t = 'std::list' if get_by_name("kind", x) == "repeated" else 'std::optional'
            d = get_by_name("default", x)
            default = f", default = {d}" if d else ''
            print (f"{t}<{fix_type(get_by_name('type', x))}> {get_by_name('name', x)}; // id = {get_by_name('id', x)}{default}")
        elif x.data == "enum":
            step_enum(x)
        elif x.data == "message":
            step_message(x.children)
    print ("};")

def step(i):
    if i.data == "package":
        print ("namespace",get_by_name("name", i),"{")
    if i.data == "message":
        step_message(i.children[0].children)

for i in x.children:
    step(i)
print ("} // namespace")
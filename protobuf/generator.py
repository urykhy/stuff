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

localTypeNames=[]

def get_encoding(name):
    xtype = {'fixed32': 'Protobuf::Walker::FIXED',
             'sint32' : 'Protobuf::Walker::ZIGZAG'
    }
    if name in xtype:
        return xtype[name]
    return None

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
    global localTypeNames
    localTypeNames.append(get_by_name('name', i))
    print (f"enum {get_by_name('name', i)} {{")
    for x in i.children[1:]:
        print (f"{get_by_name('name', x)} = {get_by_name('value', x)};")
    print ("};")

def step_message(i):
    global localTypeNames
    messageName = None

    # Data members
    for x in i:
        if x.data == "name":
            messageName = x.children[0]
            localTypeNames.append(messageName)
            print (f"struct {messageName} {{")
        elif x.data == "entry":
            t = 'std::list' if get_by_name("kind", x) == "repeated" else 'std::optional'
            print (f"{t}<{fix_type(get_by_name('type', x))}> {get_by_name('name', x)}; // id = {get_by_name('id', x)}")
        elif x.data == "enum":
            step_enum(x)
        elif x.data == "message":
            step_message(x.children)

    # Constructor
    print (f"{messageName}() ")
    for x in i:
        if x.data == "entry":
            d = get_by_name("default", x)
            if d:
                print (f": {get_by_name('name', x)}{{ {d} }}")
    print ("{}")

    # Clear
    print ("void Clear() {")
    for x in i:
        if x.data == "entry":
            d = get_by_name("default", x)
            c = ".clear()" if get_by_name("kind", x) == "repeated" else " = std::nullopt_t"
            if d:
                c = f" = {d}"
            print (f"{get_by_name('name', x)} {c};")
    print ("}")

    # ParseFromString
    print ("void ParseFromString(const Protobuf::Buffer& aString) {")
    print ("Protobuf::Walker sWalker(aString);")
    print ("sWalker.parse([this](const Protobuf::FieldInfo& aField, Protobuf::Walker* aWalker) -> Protobuf::Action {")
    print ("switch (aField.id) {")
    for x in i:
        if x.data == "entry":
            n   = get_by_name('name', x)
            t   = fix_type(get_by_name('type', x))
            enc = get_encoding(t)
            enc = f", {enc}" if enc else ''
            print (f"case {get_by_name('id', x)}:")
            if t in localTypeNames:
                print (f"{{ Protobuf::Buffer sTmpBuf; aWalker->read(sTmpBuf); {n}.push_back({t}{{}}); {n}.back().ParseFromString(sTmpBuf); }}")
            else:
                print (f"{{ {t} sTmp; aWalker->read(sTmp{enc}); {n} = std::move(sTmp); return Protobuf::ACT_USED; }}")
    print ("} return Protobuf::ACT_BREAK; });")
    print ("}")
    print ("};")

def step(i):
    if i.data == "package":
        print ("namespace",get_by_name("name", i),"{")
    if i.data == "message":
        step_message(i.children[0].children)

for i in x.children:
    step(i)
print ("} // namespace")
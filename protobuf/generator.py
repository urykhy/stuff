#!/usr/bin/env python3

import io
import sys
from lark import Lark

parser = Lark(r"""
    start        : cmd+
    cmd          : "syntax" "=" "\"" STRING "\"" SEMICOLON -> syntax
                 | "package" name SEMICOLON                -> package
                 | "option" name "=" STRING SEMICOLON
                 | message -> message
                 | enum    -> enum

    message      : "message" name "{" (entry|enum|message)+ "}"
    entry        : kind type name "=" id packed? default? SEMICOLON comment?
    enum         : "enum" name "{" enum_entry+ "}"
    enum_entry   : name "=" value SEMICOLON

    default      : "[" "default" "=" DEFAULT "]"
    packed       : "[" "packed" "=" STRING "]"
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
    DEFAULT      : /[a-zA-Z0-9._-]+/
    %import common.CNAME            -> STRING
    %import common.SIGNED_NUMBER    -> NUMBER
    %import common.WS
    %ignore WS
""")

data = sys.stdin.read()

x = parser.parse(data)
#print ( x.pretty() )
#print ("-"*120)

localTypeNames=[]
localEnums=[]

def get_by_name(name, x):
    for i in x.children:
        if i == ";":
            continue
        if name == i.data:
            return i.children[0]
    return None

class Entry:
    def __init__(self, i):
        self.id         = get_by_name('id', i)
        self.name       = get_by_name('name', i)
        self.value      = get_by_name('value', i)
        self.default    = get_by_name("default", i)
        self.kind       = get_by_name("kind", i)
        self.proto_type = get_by_name("type", i)
        self.packed     = get_by_name("packed", i) == "true"
        self.cxx_type   = self._type(self.proto_type)
        self.encoding   = self._encoding(self.proto_type)
        self.container  = 'std::pmr::list' if self.kind == "repeated" else 'std::optional'
        self.pmr        = True if self.proto_type == "string" or self.proto_type == "bytes" or self._customType() else False

    def _customType(self):
        global localTypeNames
        return self.proto_type in localTypeNames

    def _encoding(self, name):
        xtype = {'fixed32': 'Protobuf::Walker::FIXED',
                 'sint32' : 'Protobuf::Walker::ZIGZAG',
                 'fixed64': 'Protobuf::Walker::FIXED',
                 'sint64' : 'Protobuf::Walker::ZIGZAG'
        }
        if name in xtype:
            return xtype[name]
        return None

    def _type(self, name):
        xtype = {'string' : 'std::pmr::string',
                 'bytes'  : 'std::pmr::string',
                 'int32'  : 'int32_t',
                 'uint32' : 'uint32_t',
                 'fixed32': 'uint32_t',
                 'sint32' : 'int32_t',
                 'int64'  : 'int64_t',
                 'uint64' : 'uint64_t',
                 'fixed64': 'uint64_t',
                 'sint64' : 'int64_t',
                 'float'  : 'float'
        }
        if name in xtype:
            return xtype[name]
        return name

    def make_decl(self):
        return f"{self.container}<{self.cxx_type}> {self.name}; // id = {self.id}"

    def make_init(self):
        if self.kind == "repeated":
            return f", {self.name}(m_Pool)"
        if self.default:
            return f", {self.name}({self.default})"
        return ''

    def make_clear(self):
        c = ".clear()" if self.kind == "repeated" else ".reset()"
        if self.default:
            c = f" = {self.default}"
        return f"{self.name} {c};"

    def make_parse(self):
        buf = io.StringIO()
        print (f"case {self.id}:", file=buf)
        #print (f"std::cout << \"parse \" << {self.id} << std::endl; ", file=buf)

        print ("{", file=buf)

        enc  = f", {self.encoding}" if self.encoding else ''
        cp = "sTmp"
        if self.proto_type in localEnums:
            cp = f"static_cast<{self.cxx_type}>(sTmp)"
            self.cxx_type = "uint32_t" # Enumerator constants must be in the range of a 32-bit integer.

        if self._customType() and self.kind == "repeated":
            print (f"Protobuf::Buffer sTmpBuf; aWalker->read(sTmpBuf); {self.name}.emplace_back(m_Pool); {self.name}.back().ParseFromString(sTmpBuf);", file=buf)
        elif self._customType():
            print (f"Protobuf::Buffer sTmpBuf; aWalker->read(sTmpBuf); {self.name}.emplace(m_Pool); {self.name}->ParseFromString(sTmpBuf);", file=buf)
        elif self.pmr and self.kind == "repeated":
            print (f"{self.name}.emplace_back({self.cxx_type}(m_Pool)); aWalker->read({self.name}.back());", file=buf)
        elif self.pmr:
            print (f"{self.name}.emplace(m_Pool); aWalker->read(*{self.name});", file=buf)
        elif self.kind == "repeated":
            if self.packed:
                print (f"Protobuf::Buffer sTmpBuf; aWalker->read(sTmpBuf); ", file=buf)
                print (f"Protobuf::Walker sWalker(sTmpBuf);", file=buf)
                print (f"while (!sWalker.empty()) ", file=buf)
                print (f"{{ {self.cxx_type} sTmp{{}}; sWalker.read(sTmp{enc}); {self.name}.push_back({cp});}}", file=buf)
            else:
                print (f"{self.cxx_type} sTmp{{}}; aWalker->read(sTmp{enc}); {self.name}.push_back({cp});", file=buf)
        else:
            print (f"{self.cxx_type} sTmp{{}}; aWalker->read(sTmp{enc}); {self.name} = {cp};", file=buf)

        print ("return Protobuf::ACT_USED; }", file=buf)

        return buf.getvalue()

def step_enum(i):
    global localTypeNames
    localEnums.append(get_by_name('name', i))
    print (f"enum {get_by_name('name', i)} {{")
    for x in i.children[1:]:
        e = Entry(x)
        print (f"{e.name} = {e.value},")
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
            print (f"std::pmr::memory_resource* m_Pool;") # FIXME: create only if really used
        elif x.data == "entry":
            e = Entry(x)
            print (e.make_decl())
        elif x.data == "enum":
            step_enum(x)
        elif x.data == "message":
            step_message(x.children)

    # Constructor
    print (f"{messageName}(std::pmr::memory_resource* aPool) ")
    print (f": m_Pool(aPool)")
    for x in i:
        if x.data == "entry":
            e = Entry(x)
            print (e.make_init())
    print ("{}")

    # Clear
    print ("void Clear() {")
    for x in i:
        if x.data == "entry":
            e = Entry(x)
            print (e.make_clear())
    print ("}")

    # ParseFromString
    print ("void ParseFromString(const Protobuf::Buffer& aString) {")
    print ("Protobuf::Walker sWalker(aString);")
    print ("sWalker.parse([this](const Protobuf::FieldInfo& aField, Protobuf::Walker* aWalker) -> Protobuf::Action {")
    print ("switch (aField.id) {")
    for x in i:
        if x.data == "entry":
            e = Entry(x)
            print (e.make_parse())
    print ("default: return Protobuf::ACT_SKIP;")
    print ("} return Protobuf::ACT_BREAK; });")
    print ("}")
    print ("};")

def step(i):
    if i.data == "package":
        print ("namespace","pmr_" + get_by_name("name", i),"{")
    if i.data == "message":
        step_message(i.children[0].children)

print ("#include <list>")
print ("#include <memory_resource>")
print ("#include <optional>")
print ("#include <string>")
for i in x.children:
    step(i)
print ("} // namespace")
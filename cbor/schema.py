#!/usr/bin/env python
# vim:ts=4:sts=4:sw=4:et

import sys
import json
import os

# FWD required if we use generated type in other message.
#

def write_hpp(j, f):
    VARY = False
    print >>f, "#pragma once"
    if "vary" in j and j["vary"]:
        print >>f, "#include <experimental/optional>"
        VARY = True
    if "include" in j:
        for i in j["include"]:
            print >>f, "#include <"+i+">"

    print >>f, "namespace",j["namespace"],"{"
    print >>f, "struct ",j["name"]," {"

    for x in j["fields"]:
        if VARY:
            print >>f, "std::experimental::optional<"+x["type"]+">",x["name"],";"
        else:
            if  x["type"].startswith("std::list<") or \
                x["type"].startswith("std::map<") or \
                x["type"].startswith("std::vector<") or \
                x["type"] == "std::string" or \
                x["type"] == "boost::string_ref" or \
                x["type"] == "cbor::binary_ref":
                print >>f, x["type"],x["name"],";"
            else:
                print >>f, x["type"],x["name"]," = 0;"

    print >>f, "void clear() {"
    for x in j["fields"]:
        if VARY:
            print >>f, x["name"]," = std::experimental::nullopt;"
        else:
            if  x["type"].startswith("std::list<") or \
                x["type"].startswith("std::map<") or \
                x["type"].startswith("std::vector<") or \
                x["type"] == "std::string" or \
                x["type"] == "boost::string_ref":
                print >>f, x["name"],".clear();"
            elif x["type"] == "cbor::binary_ref":
                print >>f, x["name"]," = cbor::binary_ref();"
            else:
                print >>f, x["name"]," = 0;"

    print >>f, "}"
    print >>f, "};"
    print >>f, "}"

    print >>f, "namespace cbor {"
    print >>f, "void write(omemstream& out, const ",j["namespace"],"::",j["name"],"& e);"
    print >>f, "void read(imemstream& in, ",j["namespace"],"::",j["name"],"& e);"
    print >>f, "}"


def write_cpp(j, f, bname):

    VARY = False
    if "vary" in j and j["vary"]:
        VARY = True

    print >>f, "#include <encoder.hpp>"
    print >>f, "#include <decoder.hpp>"
    print >>f, "#include <"+bname+".hpp>"
    print >>f, "namespace cbor {"

    print >>f, "void write(omemstream& out, const ",j["namespace"],"::",j["name"],"& e) {"
    if VARY:
        print >>f, "size_t count = 0;"
        for x in j["fields"]:
            print >>f, "if (e.",x["name"],") {count++;}"
        print >>f, "cbor::write_type_value(out, 5, count);"

        for x in j["fields"]:
            print >>f, "if (e.",x["name"],") {"
            print >>f, "cbor::write(out, (uint32_t)",x["id"],");"
            print >>f, "cbor::write(out,e.",x["name"],".value());"
            print >>f, "}"
    else:
        print >>f, "cbor::write_type_value(out, 4, ",len(j["fields"]),");"
        for x in j["fields"]:
            if "tag" in x:
                print >>f, "write_tag(out, ",x["tag"],");"
            print >>f, "write(out,e.",x["name"],");"
    print >>f, "}"

    print >>f, "void read(imemstream& in, ",j["namespace"],"::",j["name"],"& e) {"
    if VARY:
        print >>f, "size_t count = cbor::get_uint(in, cbor::ensure_type(in, 5));"
        print >>f, "for (size_t i = 0; i < count; i++) {"
        print >>f, "uint32_t id = 0;"
        print >>f, "cbor::read(in, id);"
        print >>f, "switch (id) {"
        for x in j["fields"]:
            print >>f, "case ",x["id"],": {",x["type"],"tmp; cbor::read(in,tmp); e.",x["name"],"= std::move(tmp);} break;"
        print >>f, "}"
        print >>f, "}"
    else:
        print >>f, "if (cbor::get_uint(in, cbor::ensure_type(in, 4)) != ",len(j["fields"]),") {"
        print >>f, "throw std::runtime_error(\"bad number of elements\");"
        print >>f, "}"
        for x in j["fields"]:
            if "tag" in x:
                print >>f, "read_tag(in, ",x["tag"],");"
            print >>f, "read(in,e.",x["name"],");"
    print >>f, "}"
    print >>f, "}"

if __name__ == '__main__':
    sname = sys.argv[1]
    dname = sys.argv[2]
    j = json.load(open(sname))
    bname = os.path.basename(os.path.splitext(sname)[0])
    write_hpp(j, open(os.path.join(dname, bname+'.hpp'), 'w'))
    write_cpp(j, open(os.path.join(dname, bname+'.cpp'), 'w'), bname)
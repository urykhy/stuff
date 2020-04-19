#!/usr/bin/env python
# vim:ts=4:sts=4:sw=4:et

# ./schema.py tutorial.json tutorial.hpp
# cat tutorial.hpp | clang-format -style=Mozilla

import sys
import json
import os

def write_hpp(j, f):
    print >>f, "#pragma once"
    print >>f, "namespace "+j["namespace"]+" {"
    print >>f, "struct "+j["name"]+" {"

    for x in j["fields"]:
        print >>f, "std::optional<"+x["type"]+"> "+x["name"]+"; // id = "+x["id"]

    print >>f, "void clear() {"
    for x in j["fields"]:
        print >>f, x["name"]+" = std::nullopt;"
    print >>f, "}"

    print >>f, "void write(cbor::omemstream& out) const {"
    print >>f, "size_t sCount = 0;"
    for x in j["fields"]:
        print >>f, "if ("+x["name"]+") {sCount++;}"
    print >>f, "cbor::write_type_value(out, cbor::CBOR_MAP, sCount);"

    for x in j["fields"]:
        print >>f, "if ("+x["name"]+") {"
        print >>f, "cbor::write(out, (uint32_t)"+x["id"]+");"
        print >>f, "cbor::write(out,"+x["name"]+".value());"
        print >>f, "}"
    print >>f, "}"

    print >>f, "void read(cbor::imemstream& in) {"
    print >>f, "size_t sCount = cbor::get_uint(in, cbor::ensure_type(in, cbor::CBOR_MAP));"
    print >>f, "for (size_t i = 0; i < sCount; i++) {"
    print >>f, "uint32_t sId = 0; cbor::read(in, sId);"
    print >>f, "switch (sId) {"
    for x in j["fields"]:
        print >>f, "case "+x["id"]+": {"+x["type"]+" sTmp; cbor::read(in, sTmp); "+x["name"]+"= std::move(sTmp); break; }"
    print >>f, "}}}"
    print >>f, "};"
    print >>f, "} // namespace"

if __name__ == '__main__':
    sname = sys.argv[1]
    dname = sys.argv[2]
    j = json.load(open(sname))
    write_hpp(j, open(dname, 'w'))
#!/usr/bin/env python3
# vim:ts=4:sts=4:sw=4:et

# ./schema.py tutorial.json tutorial.hpp
# cat tutorial.hpp | clang-format -style=Mozilla

import sys
import json
import os

def write_hpp(j, f):
    print ("#pragma once", file=f)
    print ("namespace "+j["namespace"]+" {", file=f)
    print ("struct "+j["name"]+" {", file=f)

    for x in j["fields"]:
        print ("std::optional<"+x["type"]+"> "+x["name"]+"; // id = "+x["id"], file=f)

    print ("void clear() {", file=f)
    for x in j["fields"]:
        print (x["name"]+" = std::nullopt;", file=f)
    print ("}", file=f)

    print ("void cbor_write(cbor::ostream& out) const {", file=f)
    print ("size_t sCount = 0;", file=f)
    for x in j["fields"]:
        print ("if ("+x["name"]+") {sCount++;}", file=f)
    print ("cbor::write_type_value(out, cbor::CBOR_MAP, sCount);", file=f)

    for x in j["fields"]:
        print ("if ("+x["name"]+") {", file=f)
        print ("cbor::write(out, (uint32_t)"+x["id"]+");", file=f)
        print ("cbor::write(out,"+x["name"]+".value());", file=f)
        print ("}", file=f)
    print ("}", file=f)

    print ("void cbor_read(cbor::istream& in) {", file=f)
    print ("size_t sCount = cbor::get_uint(in, cbor::ensure_type(in, cbor::CBOR_MAP));", file=f)
    print ("for (size_t i = 0; i < sCount; i++) {", file=f)
    print ("uint32_t sId = 0; cbor::read(in, sId);", file=f)
    print ("switch (sId) {", file=f)
    for x in j["fields"]:
        print ("case "+x["id"]+": {"+x["type"]+" sTmp; cbor::read(in, sTmp); "+x["name"]+"= std::move(sTmp); break; }", file=f)
    print ("}}}", file=f)
    print ("};", file=f)
    print ("} // namespace", file=f)

if __name__ == '__main__':
    sname = sys.argv[1]
    dname = sys.argv[2]
    j = json.load(open(sname))
    write_hpp(j, open(dname, 'w'))
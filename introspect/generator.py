#!/usr/bin/env python

from clang.cindex import Config, Index, Cursor, CursorKind
import sys


def gen_tuple(ns, i):
    for t in ("", "const"):
        print(
            f"auto {ns}::{i.spelling}::__introspect() {t} {{ \n return std::make_tuple("
        )
        comma = False
        for j in i.get_children():
            if j.kind == CursorKind.FIELD_DECL:
                if not comma:
                    comma = True
                else:
                    print(f", ")
                print(f'std::make_pair("{j.spelling}", std::ref({j.spelling}))')
        print(f"); }}")


K = [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL, CursorKind.NAMESPACE]


def walk(ns, items):
    for i in items:
        if i.kind not in K:
            continue
        # print(f"check {i.spelling}")
        if i.kind == CursorKind.NAMESPACE:
            walk(ns + "::" + i.spelling, i.get_children())
        if any(map(lambda x: x.spelling == "__introspect", i.get_children())):
            gen_tuple(ns, i)


Config.set_library_file(
    "/usr/lib/x86_64-linux-gnu/libclang-15.so.1"
)  # FIXME: autodetect
index = Index.create()
translation_unit = index.parse(sys.argv[1], args=["-std=c++17"])
walk("", translation_unit.cursor.get_children())

#!/bin/bash

rm default.prodata default.profraw
   meson test fuzz \
&& llvm-profdata-15 merge -sparse default.profraw -o default.prodata \
&& llvm-cov-15 show ./f.out -instr-profile=default.prodata -name-regex '4cbor.*read' -Xdemangler c++filt \
&& llvm-cov-15 report ./f.out -instr-profile=default.prodata ../cbor-*.hpp

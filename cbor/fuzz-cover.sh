#!/bin/bash

rm default.prodata default.profraw
   meson test fuzz \
&& llvm-profdata-19 merge -sparse default.profraw -o default.prodata \
&& llvm-cov-19 show ./f.out -instr-profile=default.prodata -name-regex '4cbor.*read' -Xdemangler c++filt \
&& llvm-cov-19 report ./f.out -instr-profile=default.prodata ../cbor-*.hpp

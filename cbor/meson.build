project('cbor', 'cpp', version : '0.1')

includes  = include_directories('..')
benchmark = dependency('benchmark', required : true)
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
gen       = find_program('schema.py', required : true)

gen          = generator(gen, output : ['@BASENAME@.hpp'], arguments : ['@INPUT@','@OUTPUT@'])
tutorial_src = gen.process('tutorial.json')
tutorial_lib = static_library('tutorial', tutorial_src)
tutorial_dep = declare_dependency(link_with : tutorial_lib, include_directories: tutorial_lib.private_dir_include())

a  = executable('a.out',  'test.cpp', dependencies : [boost, tutorial_dep], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

# play with clang fuzzer/coverage
# meson test fuzz
# meson compile fuzz-cover
if meson.get_compiler('cpp').get_id() == 'clang'
    flags = ['-fsanitize=fuzzer,address,signed-integer-overflow', '-fprofile-instr-generate', '-fcoverage-mapping']
    f = executable('f.out', 'fuzz.cpp', cpp_args: flags, link_args: flags, include_directories : includes)
    test('fuzz', f, args : ['-jobs=3', '-max_total_time=10','-use_value_profile=1','-max_len=128'])

    fuzz_cover = find_program('fuzz-cover.sh', required : true)
    run_target('fuzz-cover', command : fuzz_cover, depends: f)
endif

project('cbor', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

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
#b  = executable('b.out',  'benchmark.cpp',  dependencies : [boost, benchmark, tutorial_dep])
#benchmark('protobuf', b)

project('allocator', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])

cpp_args = []
if meson.get_compiler('cpp').has_header('valgrind/valgrind.h')
    cpp_args = '-DCONFIG_WITH_VALGRIND'
endif

a = executable('a.out', 'test.cpp', dependencies : [boost], include_directories : includes, cpp_args : cpp_args)
test('basic', a, args : ['-l', 'all'])

compiler  = meson.get_compiler('cpp')
dl        = compiler.find_library('dl')
a = executable('t.out', 'tagged.cpp', dependencies : [boost, dl], include_directories : includes, cpp_args : cpp_args)
test('tagged test', a, args : ['-l', 'all'])

project('allocator', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])
add_languages('c')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])

cpp_args = []
if meson.get_compiler('cpp').has_header('valgrind/valgrind.h')
    cpp_args = '-DCONFIG_WITH_VALGRIND'
endif

a = executable('a.out', 'test.cpp', dependencies : [boost], include_directories : includes, cpp_args : cpp_args)
test('basic test', a, args : ['-l', 'all'])

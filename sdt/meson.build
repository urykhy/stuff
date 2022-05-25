project('sdt', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')

cxx       = meson.get_compiler('cpp')
cxx.has_header('sys/sdt.h', required: true)

a = executable('a.out', 'test.cpp', dependencies : [boost, threads], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

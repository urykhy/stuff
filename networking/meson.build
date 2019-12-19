project('networking', 'cpp', version : '0.1')

includes = include_directories('..')
add_project_arguments('-ggdb', '-O0', '-std=c++17', language: 'cpp')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
log4cxx   = dependency('liblog4cxx')
compiler  = meson.get_compiler('cpp')
resolver  = compiler.find_library('resolv')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, resolver, log4cxx], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

project('sentry', 'cpp', version : '0.1')

includes = include_directories('..')
add_project_arguments('-ggdb', '-O0', '-std=c++17', language: 'cpp')

boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'stacktrace_backtrace'])
threads   = dependency('threads')
uuid      = dependency('uuid')
json      = dependency('jsoncpp')
compiler  = meson.get_compiler('cpp')
dl        = compiler.find_library('dl')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, uuid, json, dl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

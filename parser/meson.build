project('parser', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
json      = dependency('jsoncpp')
benchmark = dependency('benchmark', required : true)

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, json], include_directories : includes)
b  = executable('b.out',  'benchmark.cpp',  dependencies : [boost, threads, json, benchmark], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])
benchmark('bench', b)
project('etcd', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
json      = dependency('jsoncpp')
threads   = dependency('threads')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, json], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

project('etcd', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
curl      = dependency('libcurl')
json      = dependency('jsoncpp')
threads   = dependency('threads')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, curl, json], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

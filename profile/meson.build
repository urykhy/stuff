project('profile', 'cpp', version : '0.1')

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
threads   = dependency('threads')
json      = dependency('jsoncpp')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, json], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

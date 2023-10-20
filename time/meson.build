project('time', 'cpp', version : '0.1')

includes = include_directories('..')
boost    = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
threads  = dependency('threads')
cctz     = dependency('cctz')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, cctz], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

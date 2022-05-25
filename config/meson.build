project('config', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine', 'filesystem'])
log4cxx   = dependency('liblog4cxx')

a = executable('a.out', 'test.cpp', dependencies : [boost, log4cxx], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

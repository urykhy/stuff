project('curl', 'cpp', version : '0.1')

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
threads   = dependency('threads')
curl      = dependency('libcurl')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, curl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

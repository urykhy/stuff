project('af_alg', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

project('parser', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')

a = executable('a.out', 'test.cpp',      dependencies : [boost, threads])
test('basic test', a, args : ['-l', 'all'])

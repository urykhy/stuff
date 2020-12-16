project('opencl', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
compute   = dependency('compute')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, compute], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

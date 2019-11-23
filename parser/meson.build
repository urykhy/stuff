project('parser', 'cpp', version : '0.1')

add_project_arguments('-ggdb', '-O3', '-std=c++17', language: 'cpp')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')

a = executable('a.out', 'test.cpp',      dependencies : [boost, threads])
test('basic test', a, args : ['-l', 'all'])

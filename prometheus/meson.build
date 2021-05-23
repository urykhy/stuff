project('stats', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem', 'coroutine'])
threads   = dependency('threads')
benchmark = dependency('benchmark', required : true)

a = executable('a.out', 'test.cpp', dependencies : [boost, threads], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

b  = executable('b.out',  'benchmark.cpp',  dependencies : [boost, benchmark], include_directories : includes)

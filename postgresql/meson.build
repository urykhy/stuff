project('postgresql', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
fmt       = dependency('fmt')
log4cxx   = dependency('liblog4cxx')
pq        = dependency('libpq')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, fmt, log4cxx, pq], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

project('sd', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine', 'program_options'])
threads   = dependency('threads')
json      = dependency('jsoncpp')
fmt       = dependency('fmt')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, json, fmt], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

d = executable('d.out', 'demo.cpp', dependencies : [boost, threads, json, fmt], include_directories : includes)
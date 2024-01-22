project('sd', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine', 'program_options'])
threads   = dependency('threads')
json      = dependency('jsoncpp')
fmt       = dependency('fmt')
xxh       = dependency('libxxhash')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, json, fmt, xxh], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

d = executable('d.out', 'demo.cpp', dependencies : [boost, threads, json, fmt, xxh], include_directories : includes)
project('mq', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
curl      = dependency('libcurl')
json      = dependency('jsoncpp')
threads   = dependency('threads')
log4cxx   = dependency('liblog4cxx')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, curl, json, log4cxx], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

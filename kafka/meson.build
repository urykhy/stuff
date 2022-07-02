project('kafka', 'cpp', version : '0.1')

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
kafka     = dependency('rdkafka++')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, kafka], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])
project('kafka', 'cpp', version : '0.1')

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
kafkapp   = dependency('rdkafka++')
kafka     = dependency('rdkafka')
log4cxx   = dependency('liblog4cxx')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, kafkapp, kafka, log4cxx], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

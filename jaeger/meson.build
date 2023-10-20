project('jaeger', 'cpp', version : '0.1')

includes = include_directories('..')

boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
uuid      = dependency('uuid')
curl      = dependency('libcurl')
log4cxx   = dependency('liblog4cxx')
subdir('otlp_proto')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, curl, uuid, otlp_dep, log4cxx], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

project('kafka', 'cpp', version : '0.1')

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
kafkapp   = dependency('rdkafka++')
kafka     = dependency('rdkafka')
log4cxx   = dependency('liblog4cxx')
benchmark = dependency('benchmark', required : true)
json      = dependency('jsoncpp')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, kafkapp, kafka, log4cxx, json], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

# meson test --benchmark -v
b  = executable('b.out',  'benchmark.cpp', dependencies : [boost, threads, kafkapp, kafka, log4cxx, benchmark], include_directories : includes)
benchmark('produce', b, timeout : 90, env : ['LOG4CXX='],
          args : ['--benchmark_min_time=1','--benchmark_color=true', '--benchmark_counters_tabular=true', '--benchmark_repetitions=3', '--benchmark_display_aggregates_only=true'])

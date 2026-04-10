project('threads', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
benchmark = dependency('benchmark', required : true)
fmt       = dependency('fmt')
log4cxx  = dependency('liblog4cxx')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, fmt, log4cxx], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

b  = executable('b.out',  'benchmark.cpp',  dependencies : [boost, threads, benchmark], include_directories : includes)
benchmark('bench', b)
#run_target('bench', command : ['./build_release/b.out', '--benchmark_repetitions=5', '--benchmark_report_aggregates_only=true'])

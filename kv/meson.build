project('kv', 'cpp', version : '0.1')

includes   = include_directories('..')
boost_test = dependency('boost', modules : ['unit_test_framework'])
benchmark  = dependency('benchmark', required : true)

subdir('asio_http')
a = executable('a.out', 'test.cpp', dependencies : [boost_test, asio_http_dep], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

# meson test --benchmark -v
b  = executable('b.out',  'benchmark.cpp', dependencies : [boost_test, asio_http_dep, benchmark], include_directories : includes)
benchmark('get', b, timeout : 90,
          args : ['--benchmark_min_time=1','--benchmark_color=true', '--benchmark_counters_tabular=true', '--benchmark_repetitions=3', '--benchmark_display_aggregates_only=true'])

project('cache', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
fdb       = dependency('foundationdb-client')
fmt       = dependency('fmt')
benchmark = dependency('benchmark', required : true)

a  = executable('a.out', 'test.cpp', dependencies : [boost, fdb, fmt], include_directories: [includes])
test('basic', a, args : ['-l', 'all'])

# meson test --benchmark -v
b  = executable('b.out',  'benchmark.cpp', dependencies : [boost, fdb, fmt, benchmark], include_directories : includes)
benchmark('get', b, timeout : 90,
          args : ['--benchmark_min_time=1','--benchmark_color=true', '--benchmark_counters_tabular=true', '--benchmark_repetitions=3', '--benchmark_display_aggregates_only=true'])
project('foundationdb', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
fdb       = dependency('foundationdb-client')
fmt       = dependency('fmt')
benchmark = dependency('benchmark', required : true)
log4cxx   = dependency('liblog4cxx')

a  = executable('a.out', 'test.cpp', dependencies : [boost, fdb, fmt, log4cxx], include_directories: [includes])
test('basic', a, args : ['-l', 'all'])

# meson test --benchmark -v
b  = executable('b.out',  'benchmark.cpp', dependencies : [boost, fdb, fmt, benchmark, log4cxx], include_directories : includes)
benchmark('get', b, timeout : 300,
          args : ['--benchmark_min_time=10','--benchmark_counters_tabular=1'])

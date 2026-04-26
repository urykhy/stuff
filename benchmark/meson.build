project('benchmark', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
fdb       = dependency('foundationdb-client')
redispp   = dependency('redis-plus-plus')
pq        = dependency('libpq')
fmt       = dependency('fmt')
benchmark = dependency('benchmark', required : true)
log4cxx   = dependency('liblog4cxx')

# meson test --benchmark -v
b  = executable('b.out',  'benchmark.cpp', dependencies : [boost, fdb, redispp, pq, fmt, benchmark, log4cxx], include_directories : includes)
benchmark('get', b, timeout : 300, env : ['LOG4CXX='],
          args : ['--benchmark_min_time=2s','--benchmark_counters_tabular=1'])

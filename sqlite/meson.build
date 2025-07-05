project('sqlite', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
threads   = dependency('threads')
log4cxx   = dependency('liblog4cxx')
sqlite    = dependency('sqlite3')
benchmark = dependency('benchmark')
xxh       = dependency('libxxhash')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, sqlite, log4cxx], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

# meson test --benchmark -v
b  = executable('b.out',  'benchmark.cpp', dependencies : [boost, threads, sqlite, log4cxx, benchmark, xxh], include_directories : includes)
benchmark('lite', b, timeout : 300,
          args : ['--benchmark_min_time=30','--benchmark_counters_tabular=1'])

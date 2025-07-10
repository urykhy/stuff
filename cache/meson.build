project('cache', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
xxh       = dependency('libxxhash')
redispp   = dependency('redis-plus-plus')
benchmark = dependency('benchmark', required : true)

a  = executable('a.out', 'test.cpp', dependencies : [boost, xxh, redispp], include_directories: [includes])
test('basic', a, args : ['-l', 'all'])

hopscotch = dependency('hopscotch', required: false)
b  = executable('b.out', 'bench.cpp', dependencies : [boost, xxh, hopscotch], include_directories: [includes])
test('s3 arc', b, args : ['-l', 'all'])

# meson test --benchmark -v
rb  = executable('rb.out',  'redis-bench.cpp', dependencies : [boost, xxh, redispp, benchmark], include_directories : includes)
benchmark('get', rb, timeout : 300,
          args : ['--benchmark_min_time=10','--benchmark_counters_tabular=1'])

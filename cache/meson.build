project('cache', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
xxh       = dependency('libxxhash')
redispp   = dependency('redis-plus-plus')
pq        = dependency('libpq')
benchmark = dependency('benchmark', required : true)
log4cxx   = dependency('liblog4cxx')

a  = executable('a.out', 'test.cpp', dependencies : [boost, xxh, redispp, log4cxx], include_directories: [includes])
test('basic', a, args : ['-l', 'all'])

hopscotch = dependency('hopscotch', required: false)
b  = executable('b.out', 'bench.cpp', dependencies : [boost, xxh, hopscotch], include_directories: [includes])
test('s3 arc', b, args : ['-l', 'all'])

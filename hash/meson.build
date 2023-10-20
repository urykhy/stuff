project('hash', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework'])
xxh       = dependency('libxxhash')
gperf     = find_program('gperf')
benchmark = dependency('benchmark')

gperf_gen = generator(gperf, output : ['@BASENAME@.hpp'], arguments : ['-t','-E','-L','C++','@INPUT@'], capture : true)
gperf_src = gperf_gen.process('gperf.in')

a = executable('a.out', 'test.cpp', gperf_src, dependencies : [boost, xxh], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

b  = executable('b.out',  'benchmark.cpp',  gperf_src, dependencies : [boost, benchmark], include_directories : includes)
benchmark('protobuf', b)
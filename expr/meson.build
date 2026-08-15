project('expr', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
benchmark = dependency('benchmark', required : true)
tcc       = dependency('tcc')
glaze     = dependency('glaze')
fmt       = dependency('fmt')
hopscotch = dependency('hopscotch')

a = executable('a.out', 'test.cpp', dependencies : [boost, tcc, glaze, fmt], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

b  = executable('b.out',  'benchmark.cpp', dependencies : [boost, tcc, glaze, fmt, hopscotch, benchmark], include_directories : includes)
benchmark('basic', b)

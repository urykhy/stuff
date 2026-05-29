project('lua', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
lua       = dependency('luajit')
sol2      = dependency('sol2')
exprtk    = dependency('exprtk')
benchmark = dependency('benchmark', required : true)

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, lua, sol2], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

b  = executable('b.out',  'benchmark.cpp', dependencies : [lua, sol2, exprtk, benchmark], include_directories : includes)
benchmark('basic', b)

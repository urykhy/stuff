project('lua', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
lua       = dependency('luajit')
sol2      = dependency('sol2')
exprtk    = dependency('exprtk')
benchmark = dependency('benchmark', required : true)
tcc       = dependency('tcc')

exprtk_lib = static_library('exprtk', 'ExprTK.cpp', dependencies : [exprtk], include_directories : includes)
exprtk_dep = declare_dependency(link_with : exprtk_lib, dependencies: [exprtk], include_directories: exprtk_lib.private_dir_include())

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, lua, sol2, tcc], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

b  = executable('b.out',  'benchmark.cpp', dependencies : [lua, sol2, exprtk_dep, tcc, benchmark], include_directories : includes)
benchmark('basic', b)

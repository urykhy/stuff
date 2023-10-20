project('avx', 'cpp', version : '0.1')
add_project_arguments('-mavx2', '-march=native', language : 'cpp')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
benchmark = dependency('benchmark', required : true)

a  = executable('a.out', 'test.cpp', dependencies : [boost], include_directories: [includes])
test('basic', a, args : ['-l', 'all'])

b  = executable('b.out',  'benchmark.cpp',  dependencies : [boost, benchmark], include_directories : includes)
benchmark('avx', b)
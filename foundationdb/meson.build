project('cache', 'cpp', version : '0.1')

includes = include_directories('..')
boost    = dependency('boost', modules : ['unit_test_framework', 'system'])
fdb      = dependency('foundationdb-client')
fmt      = dependency('fmt')

a  = executable('a.out', 'test.cpp', dependencies : [boost, fdb, fmt], include_directories: [includes])
test('basic', a, args : ['-l', 'all'])

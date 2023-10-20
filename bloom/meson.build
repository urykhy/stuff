project('bloom', 'cpp', version : '0.1')

includes = include_directories('..')
boost    = dependency('boost', modules : ['unit_test_framework', 'system', 'iostreams'])
xxh      = dependency('libxxhash')

a  = executable('a.out', 'test.cpp', dependencies : [xxh, boost], include_directories: [includes])
test('basic', a, args : ['-l', 'all'])

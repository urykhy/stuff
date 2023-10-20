project('ssl', 'cpp', version : '0.1')

includes = include_directories('..')
boost    = dependency('boost', modules : ['unit_test_framework', 'system'])
ssl      = dependency('openssl')

a  = executable('a.out', 'test.cpp', dependencies : [boost, ssl], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

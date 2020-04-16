project('bloom', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes = include_directories('..')
boost    = dependency('boost', modules : ['unit_test_framework', 'system', 'iostreams'])
ssl      = dependency('openssl')
lz4      = dependency('liblz4')

a  = executable('a.out', 'test.cpp', dependencies : [ssl, lz4, boost], include_directories: [includes])
test('basic test', a, args : ['-l', 'all'])

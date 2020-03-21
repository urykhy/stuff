project('protobuf', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])

a  = executable('a.out', 'test.cpp', dependencies : [boost])
test('basic test', a, args : ['-l', 'all'])

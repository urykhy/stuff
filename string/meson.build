project('string', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework'])

a = executable('a.out', 'test.cpp', dependencies : [boost], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

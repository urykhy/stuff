project('file', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem', 'iostreams'])
threads   = dependency('threads')
lz4       = dependency('liblz4')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, lz4], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

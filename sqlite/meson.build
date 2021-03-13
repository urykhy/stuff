project('sqlite', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])

includes = include_directories('..')
boost    = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
threads  = dependency('threads')
log4cxx  = dependency('liblog4cxx')
sqlite   = dependency('sqlite3')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, sqlite, log4cxx], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

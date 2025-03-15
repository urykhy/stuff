project('mysqld', 'cpp', version : '0.1')
add_languages('c')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
mysql     = dependency('mysqlclient')
log4cxx   = dependency('liblog4cxx')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, mysql, log4cxx], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

project('mysqld', 'cpp', version : '0.1')
add_languages('c')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
mysql     = dependency('mysqlclient')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, mysql], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

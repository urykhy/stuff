# PKG_CONFIG_PATH=../pkgconfig meson build
project('mysql', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
threads   = dependency('threads')
cctz      = dependency('cctz')
mariadb   = dependency('mariadb')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, cctz, mariadb], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

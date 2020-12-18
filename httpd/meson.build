project('httpd', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
threads   = dependency('threads')
curl      = dependency('libcurl')
subdir('http_parser')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, http_parser, curl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

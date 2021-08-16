project('jwt', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])
add_languages('c')

includes = include_directories('..')
json     = dependency('jsoncpp')
ssl      = dependency('openssl')
boost    = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads  = dependency('threads')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, json, ssl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

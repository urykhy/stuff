project('http', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])
add_languages('c')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
curl      = dependency('libcurl')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, curl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

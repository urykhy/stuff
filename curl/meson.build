project('curl', 'cpp', version : '0.1')

includes = include_directories('..')
add_project_arguments('-ggdb', '-O0', '-std=c++17', language: 'cpp')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
curl      = dependency('libcurl')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, curl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

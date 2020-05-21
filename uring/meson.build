project('uring', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])
add_languages('c')

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
uring     = dependency('liburing')

parser_src = ['../httpd/http_parser.h','../httpd/http_parser.c']
parser_lib = static_library('http_parser', parser_src)
parser     = declare_dependency(link_with : parser_lib)

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, uring, parser], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

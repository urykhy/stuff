project('httpd', 'cpp', version : '0.1')
add_languages('c')

includes = include_directories('..')
add_project_arguments('-ggdb', '-O0', '-std=c++17', language: 'cpp')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
curl      = dependency('libcurl')

parser_src = ['http_parser.h','http_parser.c']
parser_lib = static_library('http_parser', parser_src)
parser     = declare_dependency(link_with : parser_lib)

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, parser, curl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])
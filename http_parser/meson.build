#project('http_parser', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])
add_languages('c')
http_parser_src = ['http_parser.h','http_parser.c']
http_parser_lib = static_library('http_parser', http_parser_src)
http_parser     = declare_dependency(link_with : http_parser_lib, include_directories: ['.'])

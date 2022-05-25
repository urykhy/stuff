#project('http_parser', 'cpp', version : '0.1')
# https://github.com/nodejs/llhttp
# llhttp-release-v6.0.6
add_languages('c')
http_parser_src = ['api.c','http.c','llhttp.h','llhttp.c']
http_parser_lib = static_library('http_parser', http_parser_src)
http_parser     = declare_dependency(link_with : http_parser_lib, include_directories: ['.'])

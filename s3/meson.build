project('s3', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes = include_directories('..')
boost    = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
threads  = dependency('threads')
curl     = dependency('libcurl')
ssl      = dependency('openssl')
cctz     = dependency('cctz')
fmt      = dependency('fmt')
cxx      = meson.get_compiler('cpp')
cxx.has_header('rapidxml/rapidxml.hpp', required: true)

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, curl, ssl, cctz, fmt], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

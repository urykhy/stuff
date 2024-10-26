project('http', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
log4cxx   = dependency('liblog4cxx')
cpp_args = ['-DBEAST_USE_STD_STRING_VIEW']

asio_lib = static_library('asio_http', 'Client.cpp', 'Server.cpp', include_directories : includes, cpp_args: cpp_args)
asio_dep = declare_dependency(link_with : asio_lib)

a = executable('a.out', 'test.cpp', dependencies : [boost, log4cxx, asio_dep], include_directories : includes, cpp_args: cpp_args)
test('basic', a, args : ['-l', 'all'])

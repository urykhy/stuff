project('http', 'cpp', version : '0.1')

includes   = include_directories('..')
boost_test = dependency('boost', modules : ['unit_test_framework'])

subdir('lib')
a = executable('a.out', 'test.cpp', dependencies : [boost_test, asio_http_dep], include_directories : includes, cpp_args: cpp_args)
test('basic', a, args : ['-l', 'all'])

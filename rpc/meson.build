project('rpc', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
json      = dependency('jsoncpp')

api_prog  = find_program('swagger.sh', required : true)
api_gen   = generator(api_prog, output : ['@BASENAME@.hpp'], arguments : ['@INPUT@','@OUTPUT@'])
api_src   = api_gen.process('common.v1.yaml', 'keyValue.v1.yaml', 'mq.v1.yaml', 'tutorial.v1.yaml')
api_lib   = static_library('api', api_src)
api_dep   = declare_dependency(link_with : api_lib, include_directories: api_lib.private_dir_include())

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, json, api_dep], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

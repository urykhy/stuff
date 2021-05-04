project('swagger', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
json      = dependency('jsoncpp')

api_prog  = find_program('swagger.sh', required : true)
api_gen   = generator(api_prog, output : ['@BASENAME@.hpp'], arguments : ['@INPUT@','@OUTPUT@'])
api_src   = api_gen.process('common.v1.yaml', 'keyValue.v1.yaml', 'mq.v1.yaml', 'tutorial.v1.yaml', 'jsonParam.v1.yaml')
api_lib   = static_library('api', api_src)
api_dep   = declare_dependency(link_with : api_lib, include_directories: api_lib.private_dir_include())

# swagger-ui in tarball
swagger_ui_get = find_program('get_swagger_ui.sh', required : true)
swagger_ui_tar = custom_target('swagger_ui_tar', output: 'swagger_ui.tar', command: [swagger_ui_get])
objcopy        = find_program('objcopy', required : true)
swagger_ui_o   = custom_target('swagger_ui_o', output: 'swagger_ui.o', input: swagger_ui_tar,
                 command: [objcopy, '--input', 'binary', '--output', 'elf64-x86-64', '--binary-architecture', 'i386:x86-64', '--rename-section', '.data=.rodata', 'swagger_ui.tar', 'swagger_ui.o'])

a = executable('a.out', 'test.cpp', swagger_ui_o, dependencies : [boost, threads, json, api_dep], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

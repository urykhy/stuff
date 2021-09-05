project('swagger', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
json      = dependency('jsoncpp')
log4cxx   = dependency('liblog4cxx')
curl      = dependency('libcurl')
uuid      = dependency('uuid')
thrift    = dependency('thrift')
ssl       = dependency('openssl')
subdir('jaeger_proto')
import('python').find_installation('python3', modules : ['jinja2'])

api_gen   = find_program('swagger.sh', required : true)
api_src   = []
foreach name : ['common.v1.yaml', 'jsonParam.v1.yaml', 'keyValue.v1.yaml', 'mq.v1.yaml', 'tutorial.v1.yaml', 'redirect.v1.yaml']
    hpp_name = name.replace('.yaml','.hpp')
    api_src += custom_target(hpp_name,
               depend_files : files('swagger.py', '_client.j2',  '_params.j2', '_response.j2', '_server.j2', 'swagger.j2'),
               output       : hpp_name,
               input        : name,
               command      : [api_gen, '@INPUT@', '@OUTPUT@'])
endforeach

# swagger-ui in tarball
swagger_ui_get = find_program('get_swagger_ui.sh', required : true)
swagger_ui_tar = custom_target('swagger_ui_tar', output: 'swagger_ui.tar', command: [swagger_ui_get])

a = executable('a.out', 'test.cpp', api_src, swagger_ui_tar, dependencies : [boost, threads, json, log4cxx, curl, uuid, thrift, jaeger_proto, ssl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])
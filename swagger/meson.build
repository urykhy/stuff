project('swagger', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
json      = dependency('jsoncpp')
log4cxx   = dependency('liblog4cxx')
curl      = dependency('libcurl')
uuid      = dependency('uuid')
thrift    = dependency('thrift')
ssl       = dependency('openssl')
nghttp2   = dependency('libnghttp2') # for hpack
subdir('jaeger_proto')
import('python').find_installation('python3', modules : ['jinja2'])
fs        = import('fs')

api_gen   = find_program('j2/swagger.sh', required : true)
api_src   = []
foreach name : ['common.v1.yaml', 'jsonParam.v1.yaml', 'keyValue.v1.yaml', 'mq.v1.yaml', 'tutorial.v1.yaml', 'redirect.v1.yaml', 'discovery.v1.yaml']
    hpp_name = fs.replace_suffix(name, '.hpp')
    cpp_name = fs.replace_suffix(name,'.cpp')
    api_src += custom_target(hpp_name,
               depend_files : files('j2/swagger.py', 'j2/_client.j2',  'j2/_params.j2', 'j2/_response.j2', 'j2/_server.j2', 'j2/swagger.j2'),
               output       : [hpp_name, cpp_name],
               input        : 'api/' + name,
               command      : [api_gen, '@INPUT@'])
endforeach

# swagger-ui in tarball
swagger_ui_get = find_program('get_swagger_ui.sh', required : true)
swagger_ui_tar = custom_target('swagger_ui_tar', output: 'swagger_ui.tar', command: [swagger_ui_get])

subdir('asio_http_lib')

a = executable('a.out', 'test.cpp', api_src, swagger_ui_tar, dependencies : [boost, threads, json, log4cxx, curl, uuid, thrift, jaeger_proto, ssl, nghttp2, asio_dep], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

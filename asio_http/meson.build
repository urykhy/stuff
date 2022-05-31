project('http', 'cpp', version : '0.1')
add_languages('c')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine', 'filesystem'])
threads   = dependency('threads')
curl      = dependency('libcurl')
log4cxx   = dependency('liblog4cxx')
nghttp2   = dependency('libnghttp2') # for hpack
json      = dependency('jsoncpp') # catapult

golang    = find_program('go')
go_env    = environment()
go_env.set('GO111MODULE', 'off')
go_server = custom_target('go_server', output: 'server', input: 'server.go', env: go_env, command: [golang, 'build', '../server.go'])

a = executable('a.out', 'test.cpp', go_server, dependencies : [boost, threads, curl, log4cxx, nghttp2, json], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

# put beast into cage
asio_lib = static_library('asio_http', 'Lib.cpp', include_directories : includes)
asio_dep = declare_dependency(link_with : asio_lib)
b = executable('b.out', 'test-lib.cpp', dependencies : [boost, threads, curl, log4cxx, nghttp2, json, asio_dep], include_directories : includes)

project('protobuf', 'cpp', version : '0.1')

includes = include_directories('..')
add_project_arguments('-ggdb', '-O0', '-std=c++14', language: 'cpp')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
protoc    = find_program('protoc', required : true)
protobuf  = dependency('protobuf', required : true)

generator = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
rpc_src  = generator.process('rpc.proto')
rpc_lib = static_library('rpc_lib', rpc_src)
rpc_dep = declare_dependency(link_with : rpc_lib, include_directories: rpc_lib.private_dir_include())

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, rpc_dep, protobuf], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

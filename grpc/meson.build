project('grpc', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'filesystem'])
protobuf  = dependency('protobuf', required : true)
grpc      = dependency('grpc++', required : true)

protoc    = find_program('protoc', required : true)
plugin    = find_program('grpc_cpp_plugin', required : true)

protoc    = generator(protoc, output : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h', '@BASENAME@.grpc.pb.cc', '@BASENAME@.grpc.pb.h'],
            arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '--grpc_out=@BUILD_DIR@', '--plugin=protoc-gen-grpc='+plugin.full_path(), '@INPUT@'])
play_src  = protoc.process('Play.proto')
play_lib  = static_library('play', play_src)
play_dep  = declare_dependency(link_with : play_lib, include_directories: play_lib.private_dir_include())

a = executable('a.out', 'test.cpp', dependencies : [boost, play_dep, protobuf, grpc], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

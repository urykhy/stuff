# proto files from https://github.com/open-telemetry/opentelemetry-proto/tree/main/opentelemetry/proto

protobuf = dependency('protobuf', required : true)
protoc   = find_program('protoc', required : true)
otlp_gen = generator(protoc, output : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'], arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
otlp_src = otlp_gen.process('common.proto', 'resource.proto', 'trace.proto')
otlp_lib = static_library('otlp_proto', otlp_src)
otlp_dep = declare_dependency(link_with : otlp_lib, dependencies: protobuf, include_directories: otlp_lib.private_dir_include())
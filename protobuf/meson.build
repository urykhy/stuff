project('protobuf', 'cpp', version : '0.1')

add_project_arguments('-ggdb', '-O3', '-std=c++14', language: 'cpp')
protoc    = find_program('protoc', required : true)
protobuf  = dependency('protobuf', required : true)
benchmark = dependency('benchmark', required : true)
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])

generator = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
tutorial_src  = generator.process('tutorial.proto')
tutorial_lib = static_library('tutorial_lib', tutorial_src)
tutorial_dep = declare_dependency(link_with : tutorial_lib, include_directories: tutorial_lib.private_dir_include())

a = executable('a.out', 'test.cpp',      dependencies : [tutorial_dep, protobuf, boost])
b = executable('b.out', 'benchmark.cpp', dependencies : [tutorial_dep, protobuf, boost, benchmark])
test('basic test', a, args : ['-l', 'all'])
benchmark('protobuf', b)
#run_target('bench', command : ['./build/b.out'])

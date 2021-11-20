project('protobuf', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes  = include_directories('..')
protoc    = find_program('protoc', required : true)
protobuf  = dependency('protobuf', required : true)
benchmark = dependency('benchmark', required : true)
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
json      = dependency('jsoncpp')
import('python').find_installation('python3', modules : ['lark','jinja2'])

protoc        = generator(protoc, output : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'], arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
tutorial_src  = protoc.process('tutorial.proto')
tutorial_lib  = static_library('tutorial', tutorial_src)
tutorial_dep  = declare_dependency(link_with : tutorial_lib, include_directories: tutorial_lib.private_dir_include())

api_gen   = find_program('generator.sh', required : true)
api_src   = []
foreach name : ['tutorial.proto']
    hpp_name = name.replace('.proto','.hpp')
    api_src += custom_target(hpp_name,
               depend_files : files('generator.py', 'protobuf.j2'),
               output       : hpp_name,
               input        : name,
               command      : [api_gen, '@INPUT@', '@OUTPUT@'])
endforeach

a  = executable('a.out',  'test.cpp',       api_src, dependencies : [tutorial_dep, protobuf, boost, json], cpp_args : '-DWITH_JSON', include_directories : includes)
b  = executable('b.out',  'benchmark.cpp',  api_src, dependencies : [tutorial_dep, protobuf, boost, benchmark], include_directories : includes)
b2 = executable('b2.out', 'benchmark2.cpp', api_src, dependencies : [tutorial_dep, protobuf, boost, benchmark], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])
benchmark('protobuf', b)
benchmark('protobuf', b2)
#run_target('bench', command : ['./build/b.out'])

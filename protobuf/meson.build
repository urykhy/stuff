project('protobuf', 'cpp', version : '0.1',  license : 'GPL-2.0')

includes  = include_directories('..')
protoc    = find_program('protoc', required : true)
protobuf  = dependency('protobuf', required : true)
benchmark = dependency('benchmark', required : true)
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
json      = dependency('jsoncpp')
exprtk    = dependency('exprtk')
gperf     = find_program('gperf')
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

cpp_args = ['-DWITH_JSON']
if get_option('reflection')
    cpp_args += '-DWITH_REFLECTION'
endif

a  = executable('a.out',  'test.cpp',       api_src, dependencies : [tutorial_dep, protobuf, boost, json, exprtk], cpp_args : cpp_args, include_directories : includes)
b  = executable('b.out',  'benchmark.cpp',  api_src, dependencies : [tutorial_dep, protobuf, boost, json, benchmark], include_directories : includes)
b2 = executable('b2.out', 'benchmark2.cpp', api_src, dependencies : [tutorial_dep, protobuf, boost, json, benchmark, exprtk], cpp_args : cpp_args, include_directories : includes)
test('basic', a, args : ['-l', 'all'])
benchmark('protobuf', b)
benchmark('protobuf', b2)
#run_target('bench', command : ['./build/b.out'])

# play with FPM packaging
conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('LICENSE', meson.project_license())
conf.set_quoted('TAG', run_command('git', 'rev-parse', '--short', 'HEAD', check: true).stdout().strip())
conf.set_quoted('SOURCE_ROOT', meson.source_root())
binaries=[]
foreach x : [a, b, b2]
    n = import('fs').name(x.full_path())
    binaries += [n + '=' + get_option('prefix') + '/' + meson.project_name() + '/' + get_option('bindir') + '/' + n]
endforeach
conf.set('BINARIES', ' '.join(binaries))
package_file = configure_file(input: 'package.sh.in', output: 'package.sh', configuration: conf)

# play with clang fuzzer/coverage
# meson test fuzz
if meson.get_compiler('cpp').get_id() == 'clang'
    flags = ['-fsanitize=fuzzer,address,signed-integer-overflow', '-fprofile-instr-generate', '-fcoverage-mapping']
    f = executable('f.out', 'fuzz.cpp', api_src, cpp_args: flags, link_args: flags, dependencies : [tutorial_dep], include_directories : includes)
    test('fuzz', f, args : ['-jobs=3', '-max_total_time=10','-use_value_profile=1'])
endif

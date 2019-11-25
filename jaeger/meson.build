project('jaeger', 'cpp', version : '0.1')

includes = include_directories('..')
add_project_arguments('-ggdb', '-O0', '-std=c++17', language: 'cpp')

boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
thrift    = dependency('thrift')
thriftgen = find_program('thrift', required : true)

jaeger1 = custom_target('jaeger.thrift',
               output  : ['jaeger_constants.cpp','jaeger_constants.h','jaeger_types.cpp','jaeger_types.h'],
               input   : 'jaeger.thrift',
               command : [thriftgen, '-out', '.', '--gen', 'cpp:no_ostream_operators,no_skeleton', '@INPUT@'],
)
jaeger2 = custom_target('agent.thrift',
               output  : ['agent_constants.cpp','agent_constants.h','agent_types.cpp','agent_types.h','Agent.cpp','Agent.h'],
               input   : 'agent.thrift',
               command : [thriftgen, '-out', '.', '--gen', 'cpp:no_ostream_operators,no_skeleton', '@INPUT@'],
)
jaeger_lib = static_library('jaeger_lib', jaeger1, jaeger2)
jaeger_dep = declare_dependency(link_with : jaeger_lib, include_directories: jaeger_lib.private_dir_include())

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, thrift, jaeger_dep], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])
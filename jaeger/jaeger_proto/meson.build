thriftgen = find_program('thrift', required : true)

jaeger_src = custom_target('jaeger.thrift',
               output  : ['jaeger_constants.cpp','jaeger_constants.h','jaeger_types.cpp','jaeger_types.h'],
               input   : 'jaeger.thrift',
               command : [thriftgen, '-out', 'jaeger_proto', '--gen', 'cpp:no_ostream_operators,no_skeleton', '@INPUT@'],
)
agent_src = custom_target('agent.thrift',
               output  : ['agent_constants.cpp','agent_constants.h','agent_types.cpp','agent_types.h','Agent.cpp','Agent.h'],
               input   : 'agent.thrift',
               command : [thriftgen, '-out', 'jaeger_proto', '--gen', 'cpp:no_ostream_operators,no_skeleton', '@INPUT@'],
)
jaeger_lib = static_library('jaeger_lib', jaeger_src, agent_src)
jaeger_proto = declare_dependency(link_with : jaeger_lib, include_directories: jaeger_lib.private_dir_include())

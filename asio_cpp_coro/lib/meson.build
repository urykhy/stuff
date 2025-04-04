includes     = include_directories('../..')
boost_system = dependency('boost', modules : ['system'])
log4cxx      = dependency('liblog4cxx')
cpp_args     = ['-DBEAST_USE_STD_STRING_VIEW']

asio_http_lib = static_library('asio_http', 'Client.cpp', 'Server.cpp', include_directories : includes, cpp_args: cpp_args, dependencies : [boost_system, log4cxx])
asio_http_dep = declare_dependency(link_with : asio_http_lib)

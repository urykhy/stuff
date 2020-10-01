project('nflog', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes = include_directories('..')
boost     = dependency('boost', modules : ['system','program_options'])
threads   = dependency('threads')
nflog     = dependency('libnetfilter_log')
log4cxx   = dependency('liblog4cxx')

nflog = executable('nflog', 'nflog.cpp', dependencies : [boost, threads, nflog, log4cxx], include_directories : includes)

project('sentry', 'cpp', version : '0.1')

includes = include_directories('..')
add_project_arguments('-ggdb', '-O0', '-std=c++17', language: 'cpp')

test      = dependency('boost', modules : ['unit_test_framework'])
boost     = dependency('boost', modules : ['system'])
threads   = dependency('threads')
uuid      = dependency('uuid')
json      = dependency('jsoncpp')
curl      = dependency('libcurl')
compiler  = meson.get_compiler('cpp')
dl        = compiler.find_library('dl')
bfd       = compiler.find_library('bfd')
dw        = compiler.find_library('dw')

cxa_src = 'Cxa.cpp'
cxa_lib = static_library('sentry_cxa', cxa_src, include_directories : includes, dependencies : [curl, dl, json, dw])
cxa_dep = declare_dependency(link_with : cxa_lib)

a = executable('a.out', 'test.cpp', dependencies : [boost, test, threads, uuid, json, curl, dl, dw], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

cxa = executable('cxa', 'test_cxa.cpp', dependencies : [boost, threads, uuid, json, cxa_dep], include_directories : includes)

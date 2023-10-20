project('sentry', 'cpp', version : '0.1')

includes = include_directories('..')

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
log4cxx   = dependency('liblog4cxx')

TAG = run_command('git', 'rev-parse', '--short', 'HEAD', check: true).stdout().strip()
add_project_arguments('-DGIT_RELEASE='+TAG, language : 'cpp')

cxa_src = 'Cxa.cpp'
cxa_lib = static_library('sentry_cxa', cxa_src, include_directories : includes, dependencies : [curl, dl, json, dw])
cxa_dep = declare_dependency(link_with : cxa_lib)

a = executable('a.out', 'test.cpp', dependencies : [boost, test, threads, uuid, json, curl, dl, dw, log4cxx], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

cxa = executable('cxa', 'test_cxa.cpp', dependencies : [boost, threads, uuid, json, cxa_dep, log4cxx], include_directories : includes)

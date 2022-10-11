project('introspection', 'cpp')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
json      = dependency('jsoncpp')
import('python').find_installation('python3', modules : ['clang'])

intro_gen   = find_program('generator.sh', required : true)
intro_src   = []
foreach name : ['Test.hpp']
    hpp_name = name.replace('.hpp','.inl')
    intro_src += custom_target(hpp_name,
               depend_files : files('generator.py'),
               output       : hpp_name,
               input        : name,
               command      : [intro_gen, '@INPUT@', '@OUTPUT@'])
endforeach

a  = executable('a.out',  'test.cpp', intro_src, dependencies : [boost, json], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])
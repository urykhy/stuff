project('drawio', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
import('python').find_installation('python3', modules : ['jinja2'])

api_gen   = find_program('generator.sh', required : true)
api_src   = []
foreach name : ['test.drawio']
    hpp_name = name.replace('.drawio','.hpp')
    api_src += custom_target(hpp_name,
               depend_files : files('generator.py', 'generator.j2'),
               output       : hpp_name,
               input        : name,
               command      : [api_gen, '@INPUT@', '@OUTPUT@'])
endforeach

a  = executable('a.out', 'test.cpp', api_src, dependencies : [boost], include_directories: [includes])
test('basic test', a, args : ['-l', 'all'])

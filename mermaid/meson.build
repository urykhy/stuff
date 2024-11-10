project('mermaid', 'cpp', version : '0.1',  license : 'GPL-3.0')

import('python').find_installation('python3', modules : ['lark','jinja2'])

etl_gen   = find_program('generator.sh', required : true)
etl_src   = []
foreach name : ['etl.md']
    hpp_name = name.replace('.md','.hpp')
    etl_src += custom_target(hpp_name,
               depend_files : files('generator.py', 'generator.j2'),
               output       : hpp_name,
               input        : name,
               command      : [etl_gen, '@INPUT@', '@OUTPUT@'])
endforeach

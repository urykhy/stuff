project('sql', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])

flex     = find_program('flex', required : true)
flex_gen = generator(flex, output : ['@BASENAME@.cpp','@BASENAME@.h'], arguments : ['--outfile=@OUTPUT0@','--header-file=@OUTPUT1@', '@INPUT@'])
flex_src = flex_gen.process('sql.l')

bison     = find_program('bison', required : true)
bison_gen = generator(bison, output : ['@BASENAME@.tab.cpp','@BASENAME@.tab.h'], arguments : ['--output=@OUTPUT0@', '--defines=@OUTPUT1@', '@INPUT@'])
bison_src = bison_gen.process('sql.y')

a = executable('a.out', flex_src, bison_src, dependencies : [boost], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

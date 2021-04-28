project('resource', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])
add_languages('c')

includes = include_directories('..')
boost    = dependency('boost', modules : ['unit_test_framework', 'system'])
threads  = dependency('threads')
objcopy  = find_program('objcopy', required : true)

obj_gen     = generator(objcopy,
              output : ['@BASENAME@.o'],
              arguments : ['--input', 'binary', '--output', 'elf64-x86-64', '--binary-architecture', 'i386:x86-64', '--rename-section', '.data=.rodata', '@INPUT@', '@BUILD_DIR@/@BASENAME@.o'])
sample_data = obj_gen.process('sample_data.data')
sample_tar  = obj_gen.process('sample_tar.tar')

a = executable('a.out', 'test.cpp', sample_data, sample_tar, dependencies : [boost, threads], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])
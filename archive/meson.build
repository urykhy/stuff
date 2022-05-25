project('archive', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'iostreams'], version : '>=1.71')
threads   = dependency('threads')

lz4       = dependency('liblz4')
zstd      = dependency('libzstd')
lzma      = dependency('liblzma')
zlib      = dependency('zlib')
bzip2     = dependency('BZip2') # via cmake

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, lz4, zstd, lzma, zlib, bzip2], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

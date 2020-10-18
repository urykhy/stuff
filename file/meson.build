project('file_v2', 'cpp', version : '0.1', default_options : ['cpp_std=c++17'])

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'], version : '>=1.71')
threads   = dependency('threads')
lz4       = dependency('liblz4')
zstd      = dependency('libzstd')
lzma      = dependency('liblzma')
zlib      = dependency('zlib')
bzip2     = dependency('BZip2')
ssl       = dependency('openssl') # for File::Block

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, lz4, zstd, lzma, zlib, bzip2, ssl], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

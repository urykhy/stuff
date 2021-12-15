project('http', 'cpp', version : '0.1', default_options : ['cpp_std=c++2a'])
add_languages('c')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine', 'filesystem'])
threads   = dependency('threads')
curl      = dependency('libcurl')
log4cxx   = dependency('liblog4cxx')
nghttp2   = dependency('libnghttp2') # for hpack

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, curl, log4cxx, nghttp2], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

project('http', 'cpp', version : '0.1')

includes   = include_directories('..')
boost_test = dependency('boost', modules : ['unit_test_framework'])
benchmark  = dependency('benchmark', required : true)

subdir('lib')
a = executable('a.out', 'test.cpp', dependencies : [boost_test, asio_http_dep], include_directories : includes, cpp_args: cpp_args)
test('basic', a, args : ['-l', 'all'])

# meson test --benchmark -v
b  = executable('b.out',  'benchmark.cpp', dependencies : [boost_test, asio_http_dep, benchmark], include_directories : includes)
benchmark('produce', b, timeout : 90, env : ['LOG4CXX='],
          args : ['--benchmark_min_time=5','--benchmark_color=true', '--benchmark_counters_tabular=true', '--benchmark_repetitions=3', '--benchmark_display_aggregates_only=true'])
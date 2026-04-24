project('postgresql', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
fmt       = dependency('fmt')
log4cxx   = dependency('liblog4cxx')
pq        = dependency('libpq')
benchmark = dependency('benchmark', required : true)

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, fmt, log4cxx, pq], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

# meson test --benchmark -v
b  = executable('b.out',  'benchmark.cpp', dependencies : [boost, threads, fmt, benchmark, log4cxx, pq], include_directories : includes)
benchmark('get', b, timeout : 300, env : ['LOG4CXX='],
          args : ['--benchmark_min_time=1','--benchmark_counters_tabular=1'])

# need tool: https://github.com/golang-migrate/migrate
# run via: ninja migrate
migrate = find_program('migrate', required : true)
run_target('migrate', command : ['bash', '-c', migrate.full_path() + ' -path @SOURCE_ROOT@/migrations -database "postgresql://${PQ_USER}:${PQ_PASS}@${PQ_HOST}/user?sslmode=disable" up'])

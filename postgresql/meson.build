project('postgresql', 'cpp', version : '0.1')

includes  = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
fmt       = dependency('fmt')
log4cxx   = dependency('liblog4cxx')
pq        = dependency('libpq')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, fmt, log4cxx, pq], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

# need tool: https://github.com/golang-migrate/migrate
# run via: ninja migrate
migrate = find_program('migrate', required : true)
run_target('migrate', command : ['bash', '-c', migrate.full_path() + ' -path @SOURCE_ROOT@/migrations -database "postgresql://${PQ_USER}:${PQ_PASS}@${PQ_HOST}/user?sslmode=disable" up'])

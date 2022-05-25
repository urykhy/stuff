# PKG_CONFIG_PATH=../pkgconfig meson build
project('mysql', 'cpp', version : '0.1')

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system'])
threads   = dependency('threads')
cctz      = dependency('cctz')
mariadb   = dependency('mariadb')
fmt       = dependency('fmt')
log4cxx   = dependency('liblog4cxx')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, cctz, mariadb, fmt, log4cxx], include_directories : includes)
test('basic test', a, args : ['-l', 'all'])

# need tool: https://github.com/golang-migrate/migrate
# run via: ninja migrate
find_program('migrate', required : true)
run_target('migrate', command : ['bash', '-c', 'migrate -path @SOURCE_ROOT@/migrations -database "mysql://${MYSQL_USER}:${MYSQL_PASS}@tcp(${MYSQL_HOST}:${MYSQL_PORT})/test" up'])
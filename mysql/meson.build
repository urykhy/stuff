# PKG_CONFIG_PATH=../pkgconfig meson build
project('mysql', 'cpp', version : '0.1')

includes = include_directories('..')
boost     = dependency('boost', modules : ['unit_test_framework', 'system', 'coroutine'])
threads   = dependency('threads')
cctz      = dependency('cctz')
mysql     = dependency('mysqlclient')
fmt       = dependency('fmt')
log4cxx   = dependency('liblog4cxx')
redispp   = dependency('redis-plus-plus')

a = executable('a.out', 'test.cpp', dependencies : [boost, threads, cctz, mysql, fmt, log4cxx, redispp], include_directories : includes)
test('basic', a, args : ['-l', 'all'])

# need tool: https://github.com/golang-migrate/migrate
# run via: ninja migrate
find_program('migrate', required : true)
run_target('migrate', command : ['bash', '-c', 'migrate -path @SOURCE_ROOT@/migrations -database "mysql://${MYSQL_USER}:${MYSQL_PASS}@tcp(${MYSQL_HOST}:${MYSQL_PORT})/test" up'])

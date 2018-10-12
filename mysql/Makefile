MYSQL_INC  =  $(shell mariadb_config --include)
MYSQL_LIB  =  $(shell mariadb_config --libs)
CCTZ_INC   = -I/usr/include/cctz
CCTZ_LIB   = -lcctz
BOOST_LIB  = -lboost_filesystem -lboost_system -lboost_unit_test_framework
STD_FLAGS  = -pthread -ggdb -std=c++14 -O3

CXXFLAGS   = $(MYSQL_INC) $(CCTZ_INC) $(STD_FLAGS) -I. -I../threads/
LDFLAGS    = $(MYSQL_LIB) $(CCTZ_LIB) $(BOOST_LIB)
.PHONY: test clean

a.out: test.cpp
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm ./a.out || true

test: a.out
	./a.out -l all -t MySQL/prepare

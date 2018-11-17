BOOST_LIB  = -lboost_system -lboost_unit_test_framework -pthread
STD_FLAGS  = -ggdb -std=c++14 -O3

CXXFLAGS   = $(STD_FLAGS) -I. -I../threads
LDFLAGS    = $(BOOST_LIB)
.PHONY: test clean all

all: a.out

a.out: test.cpp
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm ./a.out || true

test: a.out
	./a.out -l all -t MQ

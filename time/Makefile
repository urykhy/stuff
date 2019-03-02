CCTZ_INC  = -I/usr/include/cctz
CCTZ_LIB  = -lcctz
BOOST_LIB  = -lboost_unit_test_framework
STD_FLAGS  = -pthread -ggdb -std=c++14 -O3

CXXFLAGS   = $(CCTZ_INC) $(STD_FLAGS) -I.
LDFLAGS    = $(CCTZ_LIB) $(BOOST_LIB)
.PHONY: test clean

a.out: test.cpp
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm ./a.out || true

test: a.out
	./a.out -l all

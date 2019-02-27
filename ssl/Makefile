SSL_INC  =  $(shell pkg-config --cflags openssl)
SSL_LIB  =  $(shell pkg-config --libs openssl)
BOOST_LIB  = -lboost_unit_test_framework
STD_FLAGS  = -pthread -ggdb -std=c++14 -O3

CXXFLAGS   = $(SSL_INC) $(STD_FLAGS) -I.
LDFLAGS    = $(SSL_LIB) $(BOOST_LIB)
.PHONY: test clean

a.out: test.cpp
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm ./a.out || true

test: a.out
	./a.out -l all

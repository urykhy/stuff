PROTOBUF_INC   = $(shell pkg-config --cflags protobuf)
PROTOBUF_LIB   = $(shell pkg-config --libs protobuf)
PROTOC = protoc
PROTOCFLAGS = --cpp_out=.
BENCHMARK_LIB  = -lbenchmark

BOOST_LIB  = -lboost_system -lboost_unit_test_framework
STD_FLAGS  = -ggdb -std=c++14 -O3

CXXFLAGS   = $(PROTOBUF_INC) $(STD_FLAGS) -I.
LDFLAGS    = $(PROTOBUF_LIB) $(BOOST_LIB)
.PHONY: test clean bench all

all: a.out b.out

a.out: test.cpp tutorial.pb.cc
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

b.out: benchmark.cpp tutorial.pb.cc
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS) $(BENCHMARK_LIB)

%.pb.cc: %.proto
	$(PROTOC) $(PROTOCFLAGS) $<

clean:
	rm ./a.out ./b.out tutorial.pb.cc tutorial.pb.h || true

test: a.out
	./a.out -l all -t Protobuf/simple

bench: b.out
	./b.out

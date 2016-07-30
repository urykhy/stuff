
# disable built-in rules
.SUFFIXES:
MAKEFLAGS += -r

CXX := g++
CXXFLAGS := -std=c++11 -O3 -ggdb -I. -Wall -W -pedantic -mtune=native

all: lr ftrl
clean:
	$(RM) lr ftrl

lr:	lr.cpp Lr.hpp Parser.hpp LrGSL.hpp
	$(CXX) $(CXXFLAGS) $< -o $@ -lgsl -lgslcblas -lboost_program_options

ftrl: ftrl.cpp Ftrl.hpp Parser.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

run: all
	./lr
	./ftrl


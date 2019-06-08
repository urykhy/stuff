X_INC      = $(shell PKG_CONFIG_PATH="." pkg-config --cflags boost_unit_test_framework boost_coroutine)
X_LIB      = $(shell PKG_CONFIG_PATH="." pkg-config --libs boost_unit_test_framework boost_coroutine)
STD_FLAGS  = -pthread -ggdb -std=c++14 -O0

CXXFLAGS   = $(X_INC) $(STD_FLAGS) -I.
LDFLAGS    = $(X_LIB)
.PHONY: test clean

a.out: test.cpp
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm ./a.out || true

test: a.out
	LD_LIBRARY_PATH=~/tmp/boost1.70/lib ./a.out -l all

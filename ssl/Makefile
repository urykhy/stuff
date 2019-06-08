X_INC      = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --cflags openssl boost_unit_test_framework)
X_LIB      = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --libs openssl boost_unit_test_framework)
STD_FLAGS  = -pthread -ggdb -std=c++14 -O3

CXXFLAGS   = $(X_INC) $(STD_FLAGS) -I.
LDFLAGS    = $(X_LIB)
.PHONY: test clean

a.out: test.cpp
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm ./a.out || true

test: a.out
	./a.out -l all

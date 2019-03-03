X_INC      = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --cflags cctz mariadb)
X_LIB      = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --libs   cctz mariadb)
BOOST_LIB  = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --libs   boost_unit_test_framework boost_filesystem boost_system)
STD_FLAGS  = -pthread -ggdb -std=c++14 -O3

CXXFLAGS   = $(X_INC) $(STD_FLAGS) -I. -I..
LDFLAGS    = $(X_LIB) $(BOOST_LIB)
.PHONY: test clean

a.out: test.cpp
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm ./a.out || true

test: a.out
	./a.out -l all -t MySQL/prepare

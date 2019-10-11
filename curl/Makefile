X_INC      = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --cflags boost_unit_test_framework libcurl)
X_LIB      = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --libs boost_unit_test_framework libcurl)
STD_FLAGS  = -pthread -ggdb -std=c++14 -O0

CXXFLAGS   = $(X_INC) $(STD_FLAGS) -I. -I..
LDFLAGS    = $(X_LIB)
.PHONY: test clean

a.out: test.cpp Curl.hpp Multi.hpp
		$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
		rm ./a.out || true

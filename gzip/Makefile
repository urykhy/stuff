X_INC      = $(shell pkg-config --cflags liblz4)
X_LIB      = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --libs liblz4)
BOOST_LIB  = $(shell PKG_CONFIG_PATH="../pkgconfig" pkg-config --libs boost_unit_test_framework boost_iostreams)
STD_FLAGS  = -pthread -ggdb -std=c++17 -O3

CXXFLAGS   = $(X_INC) $(STD_FLAGS) -I. -I..
LDFLAGS    = $(X_LIB) $(BOOST_LIB)
.PHONY: test clean

a.out: test.cpp __static.cpp
	$(CXX) $< -o $@ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm ./a.out || true

test: a.out
	./a.out -l all

__static.cpp:
	echo "" > $@
	echo "test stream" | gzip  > test.gz  && xxd -i test.gz  >> $@ && rm test.gz
	echo "test stream" | bzip2 > test.bz2 && xxd -i test.bz2 >> $@ && rm test.bz2
	echo "test stream" | xz    > test.xz  && xxd -i test.xz  >> $@ && rm test.xz
	echo "test stream" | lz4   > test.lz4 && xxd -i test.lz4 >> $@ && rm test.lz4
	echo "test stream" | zstd  > test.zst && xxd -i test.zst >> $@ && rm test.zst
	echo "test stream" | gzip  > concat.gz && echo "other line" | gzip >> concat.gz && xxd -i concat.gz >> $@ && rm concat.gz

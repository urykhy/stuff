
CXX := g++
CXXFLAGS := -std=c++14 -ggdb -Og -Wall -pedantic -I. -fPIC
LDFLAGS := -ldl -L. -l_user -l_logger

.PHONY: all run clean

all: test_logger
run: test_logger
		LD_LIBRARY_PATH=. ./test_logger
clean:
		rm test_logger *.o *.so || true

%.o : %.cpp
		$(CXX) ${CXXFLAGS} -c $< -o $@

%.so: %.o
		$(CXX) -shared -Wl,-soname,$@ -o $@ $<

test_logger: lib_user.so
test_logger: lib_logger.so
test_logger: test-logger.o
		$(CXX) $< -o $@ $(LDFLAGS)



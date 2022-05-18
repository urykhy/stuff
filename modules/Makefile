CXX := clang++-15
CXXFLAGS += -std=c++2a

all: a.out
clean:
	-rm *.pcm *.o a.out

# create module from std header
std.pcm: std.modulemap std.hpp
	$(CXX) $(CXXFLAGS) -Xclang -emit-module -fmodule-name=std -xc++ -c $< -o $@

# create module (which in turn use std module)
foo.pcm: foo.hpp
	$(CXX) $(CXXFLAGS) -Xclang -emit-module-interface -c $< -o $@
foo.o: foo.cpp foo.pcm std.pcm
	$(CXX) $(CXXFLAGS) -c $< -o $@ -fmodule-file=foo.pcm -fprebuilt-module-path=.

# use module
main.o: main.cpp foo.pcm
	$(CXX) $(CXXFLAGS) -c $< -o $@ -fprebuilt-module-path=.
a.out: main.o foo.o
	$(CXX) $(CXXFLAGS) $^ -o $@

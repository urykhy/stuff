
# disable built-in rules
.SUFFIXES:
MAKEFLAGS += -r

LIBPARSER  = .build/libparser.a
PARSER_SRC = src/HttpParser.cpp src/http_parser.cpp
PARSER_O   = $(patsubst %.cpp,.build/%.o,$(PARSER_SRC))

CXXFLAGS   = -std=c++14 -Iinclude -O0 -ggdb -pedantic -W -Wall -pthread
LDFLAGS    = -pthread -lboost_system -lboost_unit_test_framework -llog4cxx $(LIBPARSER)
TARGETS    = $(patsubst %.cpp,%,$(wildcard test*.cpp))

$(TARGETS): $(LIBPARSER)

.PHONY: all clean
all: $(TARGETS)
clean:
	rm -r .build .compiler_flags $(TARGETS) || true

%: .build/%.o
	$(CXX) $< -o $@ $(LDFLAGS)

$(LIBPARSER): $(PARSER_O)
	ar vrcs $(LIBPARSER) $^

##
## dependency generation from http://make.paulandlesley.org/autodep.html
##
SUFFIXES += .d
NODEPS:=clean tags
SOURCES:=$(shell find . -name "*.cpp" -printf "%f\n")
DEPFILES:=$(patsubst %.cpp,.build/%.d,$(SOURCES))
#Don't create dependencies when we're cleaning, for instance
ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
    #Chances are, these files don't exist.  GMake will create them and
    #clean up automatically afterwards
    -include $(DEPFILES)
endif
.build:
	mkdir -p $@
	mkdir -p $@/src
.build/%.d: %.cpp .compiler_flags | .build
	$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cpp,.build/%.o,$<)' $< -MF $@
.build/%.o: %.cpp .build/%.d
	$(CXX) $(CXXFLAGS) -c $< -o $@

# rebuild if CXXFLAGS changed
.PHONY: force
.compiler_flags: force
	@echo '$(CXXFLAGS)' | cmp -s - $@ || echo '$(CXXFLAGS)' > $@

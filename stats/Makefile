
# disable built-in rules
.SUFFIXES:
MAKEFLAGS += -r

CXXFLAGS  = -std=c++14 -I. -O0 -ggdb -pedantic -W -Wall -pthread
LDFLAGS   = -pthread -lboost_unit_test_framework
GENERATED = $(patsubst %.yml,%.hpp,$(wildcard *.yml))
TARGETS   = $(patsubst %.cpp,%,$(wildcard test*.cpp))

.PHONY: all clean
all: $(TARGETS)
clean:
	rm -r .build $(TARGETS) $(GENERATED) || true

%.hpp: %.yml parser.py
	@./parser.py > $@

.PRECIOUS: $(GENERATED)
%: .build/%.o
	$(CXX) $(LDFLAGS) $< -o $@

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
.build/%.d: %.cpp .compiler_flags $(GENERATED) | .build
	$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cpp,.build/%.o,$<)' $< -MF $@
.build/%.o: %.cpp .build/%.d
	$(CXX) $(CXXFLAGS) -c $< -o $@

# rebuild if CXXFLAGS changed
.PHONY: force
.compiler_flags: force
	@echo '$(CXXFLAGS)' | cmp -s - $@ || echo '$(CXXFLAGS)' > $@


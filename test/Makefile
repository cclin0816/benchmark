.PHONY: all clean test

CXX ?= clang++
CXXFLAGS ?= -O1 -Wall -Wextra -Wshadow -Wpedantic -Werror
CXXSTD ?= c++11

all: test_binding

test_binding: test_binding.cpp Makefile ../benchmark.h
	$(CXX) $(CXXFLAGS) -std=$(CXXSTD) -o test_binding test_binding.cpp

test:
	./test_binding

clean:
	rm -f test_binding
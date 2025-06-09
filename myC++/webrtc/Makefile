# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++23
LDFLAGS = -ldatachannel -lpthread

TARGETS = example

all: $(TARGETS)

example: example.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

test: all
	@echo "### TESTING ###"
	./example

clean:
	rm -f $(TARGETS)


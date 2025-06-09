# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++23
LDFLAGS = -pthread

TARGETS = subscriber publisher

all: $(TARGETS)

subscriber: subscriber.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

publisher: publisher.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

test: all
	@echo "Run ./publisher and ./subscriber in 2 terminals"

clean:
	rm -f $(TARGETS)


# Compiler flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++23
TARGETS = chat_server chat_client

all: $(TARGETS)

chat_server: chat_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

chat_client: chat_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: all
	@echo "Run ./chat_server and ./chat_client in 2 terminals and type"

clean:
	rm -f $(TARGETS)


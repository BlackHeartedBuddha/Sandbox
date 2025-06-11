# Makefile for SDL and V4L2 video streaming clients

CXX = g++
CXXFLAGS = -Wall -O2
LDFLAGS_SDL = -ljpeg -lSDL2
LDFLAGS_V4L2 = -ljpeg

# Targets
TARGETS = sdl_tcp_client sdl_udp_client v4l2_udp_stream v4l2_tcp_stream

all: $(TARGETS)

sdl_tcp_client: sdl_tcp_client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS_SDL)

v4l2_tcp_stream: v4l2_tcp_stream.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS_V4L2)

v4l2_udp_stream: v4l2_udp_stream.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS_V4L2)

sdl_udp_client: sdl_udp_client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ -lSDL2 -ljpeg

clean:
	rm -f $(TARGETS)


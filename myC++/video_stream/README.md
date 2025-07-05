# C++ implementation of video stream over tcp

Need to develop proper client code to render efficiently

```bash

## Commands
 6284* g++ v4l2_tcp_stream.cpp -o v4l2_tcp_stream -ljpeg\n
 6285* ./v4l2_tcp_stream

```

## Improvements
- Persistent rgb + JPEG buffers	Reduces per-frame malloc/free
- Multiple V4L2 buffers (≥4)	Enables pipelining
- select() or poll()	Avoids blocking delays
- MJPEG capture format	Skips software JPEG encoding
- TurboJPEG	Faster JPEG compression
- Socket send buffer tuning	Avoids send stalls

## TODO:
- [ ] Implement udp version, verify possible compression solution and computation cost on the device
- [ ] Compare with webrtc performance
- [ ] Implement efficient

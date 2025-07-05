#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <netinet/tcp.h>

#include <iostream>
#include <cstring>
#include <cstdlib>

#define WIDTH 640
#define HEIGHT 480
#define TCP_PORT 8080
#define BUFFER_COUNT 4

struct buffer {
    void* start;
    size_t length;
};

// Helper to send all data (handle partial sends)
ssize_t send_all(int sockfd, const void* data, size_t length) {
    size_t total_sent = 0;
    const char* buf = static_cast<const char*>(data);
    while (total_sent < length) {
        ssize_t sent = send(sockfd, buf + total_sent, length - total_sent, 0);
        if (sent <= 0) {
            if (sent < 0 && (errno == EINTR || errno == EAGAIN))
                continue; // retry
            return -1; // error or disconnected
        }
        total_sent += sent;
    }
    return total_sent;
}

int main() {
    const char* device = "/dev/video0";
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("Cannot open device");
        return 1;
    }

    // Set MJPEG format
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("Setting MJPEG format failed");
        close(fd);
        return 1;
    }

    // Request buffers
    v4l2_requestbuffers req{};
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("Requesting buffers");
        close(fd);
        return 1;
    }

    buffer* buffers = new buffer[req.count];
    for (int i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("Querying buffer");
            // Cleanup
            for (int j = 0; j < i; ++j)
                munmap(buffers[j].start, buffers[j].length);
            delete[] buffers;
            close(fd);
            return 1;
        }
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            for (int j = 0; j < i; ++j)
                munmap(buffers[j].start, buffers[j].length);
            delete[] buffers;
            close(fd);
            return 1;
        }
    }

    // Queue all buffers
    for (int i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("Queue buffer");
            // Cleanup omitted for brevity
            // You should handle properly in production code
            return 1;
        }
    }

    // Start streaming
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("Start capture");
        // Cleanup omitted
        return 1;
    }

    // Setup TCP server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(TCP_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 1) < 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    std::cout << "Waiting for client on port " << TCP_PORT << "...\n";

    int clientfd = accept(sockfd, nullptr, nullptr);
    if (clientfd < 0) {
        perror("accept");
        close(sockfd);
        return 1;
    }
    std::cout << "Client connected.\n";

    // Set TCP_NODELAY for low latency
    int flag = 1;
    setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = POLLIN;

    while (true) {
        int ret = poll(fds, 1, -1);
        if (ret < 0) {
            perror("poll");
            break;
        }
        if (fds[0].revents & POLLIN) {
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
                perror("VIDIOC_DQBUF");
                break;
            }

            // Send frame size first (network byte order)
            uint32_t size_net = htonl(buf.bytesused);
            if (send_all(clientfd, &size_net, sizeof(size_net)) < 0) {
                perror("send size");
                break;
            }

            // Send MJPEG frame
            if (send_all(clientfd, buffers[buf.index].start, buf.bytesused) < 0) {
                perror("send frame");
                break;
            }

            // Requeue buffer
            if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
                perror("VIDIOC_QBUF");
                break;
            }
        }
    }

    // Cleanup
    for (int i = 0; i < req.count; ++i)
        munmap(buffers[i].start, buffers[i].length);
    delete[] buffers;
    close(clientfd);
    close(sockfd);
    close(fd);

    return 0;
}


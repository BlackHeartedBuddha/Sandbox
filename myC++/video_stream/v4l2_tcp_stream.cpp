#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <jpeglib.h>

struct buffer {
    void* start;
    size_t length;
};

int main() {
    const char* device = "/dev/video0";
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("Cannot open device");
        return 1;
    }

    // Set format
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // common format, raw YUV
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("Setting Pixel Format");
        return 1;
    }

    // Request buffers
    v4l2_requestbuffers req{};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("Requesting Buffer");
        return 1;
    }

    // Map buffer
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        perror("Querying Buffer");
        return 1;
    }

    buffer buffer_info;
    buffer_info.length = buf.length;
    buffer_info.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    if (buffer_info.start == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Start streaming
    int type = buf.type;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("Start Capture");
        return 1;
    }

    // Setup TCP server
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        return 1;
    }

    listen(sockfd, 1);
    std::cout << "Waiting for client on port 8080...\n";
    int clientfd = accept(sockfd, nullptr, nullptr);
    if (clientfd < 0) {
        perror("accept");
        return 1;
    }
    std::cout << "Client connected!\n";

    // JPEG compression setup
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    JSAMPROW row_pointer[1];
    unsigned char* jpeg_buf = nullptr;
    unsigned long jpeg_size = 0;

    while (true) {
        // Queue buffer for capture
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            break;
        }

        // Dequeue buffer (wait for frame)
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("VIDIOC_DQBUF");
            break;
        }

        // Convert YUYV to RGB24
        unsigned char* yuyv = static_cast<unsigned char*>(buffer_info.start);
        int width = fmt.fmt.pix.width;
        int height = fmt.fmt.pix.height;
        unsigned char* rgb = new unsigned char[width * height * 3];

        for (int i = 0, j = 0; i < width * height * 2; i += 4, j += 6) {
            int y0 = yuyv[i + 0] - 16;
            int u  = yuyv[i + 1] - 128;
            int y1 = yuyv[i + 2] - 16;
            int v  = yuyv[i + 3] - 128;

            auto clamp = [](int val) {
                if (val < 0) return 0;
                if (val > 255) return 255;
                return val;
            };

            int r = clamp((298 * y0 + 409 * v + 128) >> 8);
            int g = clamp((298 * y0 - 100 * u - 208 * v + 128) >> 8);
            int b = clamp((298 * y0 + 516 * u + 128) >> 8);

            rgb[j + 0] = r;
            rgb[j + 1] = g;
            rgb[j + 2] = b;

            r = clamp((298 * y1 + 409 * v + 128) >> 8);
            g = clamp((298 * y1 - 100 * u - 208 * v + 128) >> 8);
            b = clamp((298 * y1 + 516 * u + 128) >> 8);

            rgb[j + 3] = r;
            rgb[j + 4] = g;
            rgb[j + 5] = b;
        }

        // Compress RGB to JPEG in memory
        jpeg_mem_dest(&cinfo, &jpeg_buf, &jpeg_size);

        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, 75, TRUE);

        jpeg_start_compress(&cinfo, TRUE);

        while (cinfo.next_scanline < cinfo.image_height) {
            row_pointer[0] = &rgb[cinfo.next_scanline * width * 3];
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);

        // Send JPEG size and data over TCP
        uint32_t size_net = htonl(jpeg_size);
        if (send(clientfd, &size_net, sizeof(size_net), 0) <= 0) break;
        if (send(clientfd, jpeg_buf, jpeg_size, 0) <= 0) break;

        // Cleanup
        free(jpeg_buf);
        jpeg_buf = nullptr;
        jpeg_size = 0;
        delete[] rgb;

        // Optional: small sleep or frame rate limit
        usleep(13000);  // ~30fps
    }

    // Cleanup
    jpeg_destroy_compress(&cinfo);
    close(clientfd);
    close(sockfd);
    munmap(buffer_info.start, buffer_info.length);
    close(fd);

    return 0;
}


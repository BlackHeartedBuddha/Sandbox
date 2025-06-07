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

#define PORT 8080
#define DEST_IP "127.0.0.1"
#define PACKET_SIZE 1400  // max UDP payload is ~1500 - IP/UDP headers

struct buffer {
    void* start;
    size_t length;
};

int main() {
    // Open camera
    const char* device = "/dev/video0";
    int fd = open(device, O_RDWR);
    if (fd < 0) { perror("Cannot open device"); return 1; }

    // Set format
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) { perror("Setting Pixel Format"); return 1; }

    // Request buffer
    v4l2_requestbuffers req{};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) { perror("Requesting Buffer"); return 1; }

    // Map buffer
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) { perror("Querying Buffer"); return 1; }

    buffer buffer_info;
    buffer_info.length = buf.length;
    buffer_info.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer_info.start == MAP_FAILED) { perror("mmap"); return 1; }

    // Start streaming
    int type = buf.type;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) { perror("Start Capture"); return 1; }

    // UDP socket setup
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    sockaddr_in client_addr{};
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, DEST_IP, &client_addr.sin_addr);

    // JPEG compression setup
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    JSAMPROW row_pointer[1];
    unsigned char* jpeg_buf = nullptr;
    unsigned long jpeg_size = 0;
    uint32_t frame_id = 0;

    while (true) {
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) { perror("VIDIOC_QBUF"); break; }
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) { perror("VIDIOC_DQBUF"); break; }

        // Convert YUYV to RGB
        unsigned char* yuyv = static_cast<unsigned char*>(buffer_info.start);
        int width = fmt.fmt.pix.width;
        int height = fmt.fmt.pix.height;
        unsigned char* rgb = new unsigned char[width * height * 3];

        for (int i = 0, j = 0; i < width * height * 2; i += 4, j += 6) {
            int y0 = yuyv[i + 0] - 16;
            int u  = yuyv[i + 1] - 128;
            int y1 = yuyv[i + 2] - 16;
            int v  = yuyv[i + 3] - 128;

            auto clamp = [](int val) { return val < 0 ? 0 : (val > 255 ? 255 : val); };

            rgb[j + 0] = clamp((298 * y0 + 409 * v + 128) >> 8);
            rgb[j + 1] = clamp((298 * y0 - 100 * u - 208 * v + 128) >> 8);
            rgb[j + 2] = clamp((298 * y0 + 516 * u + 128) >> 8);
            rgb[j + 3] = clamp((298 * y1 + 409 * v + 128) >> 8);
            rgb[j + 4] = clamp((298 * y1 - 100 * u - 208 * v + 128) >> 8);
            rgb[j + 5] = clamp((298 * y1 + 516 * u + 128) >> 8);
        }

        // Compress to JPEG
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

        // Send in chunks with header
        size_t max_data = PACKET_SIZE - 8; // 4 bytes frame_id, 2 bytes total_parts, 2 bytes part_index
        int total_parts = (jpeg_size + max_data - 1) / max_data;

        for (int i = 0; i < total_parts; ++i) {
            size_t chunk_size = (i < total_parts - 1) ? max_data : (jpeg_size - i * max_data);
            char packet[PACKET_SIZE];

            // Header: [frame_id (4)] [total_parts (2)] [part_index (2)]
            uint32_t fid = htonl(frame_id);
            uint16_t parts = htons(total_parts);
            uint16_t index = htons(i);
            memcpy(packet, &fid, 4);
            memcpy(packet + 4, &parts, 2);
            memcpy(packet + 6, &index, 2);
            memcpy(packet + 8, jpeg_buf + i * max_data, chunk_size);

            sendto(sockfd, packet, chunk_size + 8, 0, (sockaddr*)&client_addr, sizeof(client_addr));
        }

        frame_id++;
        free(jpeg_buf);
        jpeg_buf = nullptr;
        jpeg_size = 0;
        delete[] rgb;
        usleep(30000); // ~30fps
    }

    // Cleanup
    jpeg_destroy_compress(&cinfo);
    munmap(buffer_info.start, buffer_info.length);
    close(fd);
    close(sockfd);
    return 0;
}


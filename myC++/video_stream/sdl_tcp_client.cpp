#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <jpeglib.h>
#include <SDL2/SDL.h>

bool recv_all(int sock, void* buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t r = recv(sock, (char*)buf + received, len - received, 0);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}

unsigned char* decode_jpeg(const unsigned char* jpeg_data, unsigned long jpeg_size, int& width, int& height) {
    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;
    int row_stride = width * cinfo.output_components;

    unsigned char* buffer = new unsigned char[width * height * 3];
    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char* row = buffer + cinfo.output_scanline * row_stride;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return buffer;
}

int main() {
    const char* host = "127.0.0.1";
    const int port = 8080;

    // Connect to server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, host, &server.sin_addr);

    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        return 1;
    }

    std::cout << "Connected to server\n";

    // SDL Init
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Video Stream", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = nullptr;

    while (true) {
        uint32_t jpeg_size_net;
        if (!recv_all(sock, &jpeg_size_net, 4)) break;
        uint32_t jpeg_size = ntohl(jpeg_size_net);

        std::vector<unsigned char> jpeg_data(jpeg_size);
        if (!recv_all(sock, jpeg_data.data(), jpeg_size)) break;

        int width = 0, height = 0;
        unsigned char* rgb = decode_jpeg(jpeg_data.data(), jpeg_size, width, height);
        if (!rgb) {
            std::cerr << "Failed to decode JPEG\n";
            continue;
        }

        if (!texture || width != 640 || height != 480) {
            if (texture) SDL_DestroyTexture(texture);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
        }

        SDL_UpdateTexture(texture, nullptr, rgb, width * 3);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        delete[] rgb;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto cleanup;
        }
    }

cleanup:
    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close(sock);
    return 0;
}


#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <jpeglib.h>
#include <SDL2/SDL.h>


#define PORT 8080
#define PACKET_SIZE 1400
#define FRAME_TIMEOUT_MS 200

struct PacketHeader {
    uint32_t frame_id;
    uint16_t total_parts;
    uint16_t part_index;
};

struct FrameBuffer {
    std::vector<std::vector<uint8_t>> parts;
    uint16_t total_parts;
    uint32_t received_parts;
    uint64_t last_update_time;
};

uint64_t current_time_ms() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("UDP Video Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* texture = nullptr;

    std::unordered_map<uint32_t, FrameBuffer> frame_map;

    char buffer[PACKET_SIZE];
    while (true) {
        ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0, nullptr, nullptr);
        if (len < 8) continue;

        PacketHeader header;
        memcpy(&header.frame_id, buffer, 4);
        memcpy(&header.total_parts, buffer + 4, 2);
        memcpy(&header.part_index, buffer + 6, 2);

        header.frame_id = ntohl(header.frame_id);
        header.total_parts = ntohs(header.total_parts);
        header.part_index = ntohs(header.part_index);

        auto& frame = frame_map[header.frame_id];
        frame.last_update_time = current_time_ms();

        if (frame.parts.empty()) {
            frame.parts.resize(header.total_parts);
            frame.total_parts = header.total_parts;
            frame.received_parts = 0;
        }

        if (header.part_index < header.total_parts && frame.parts[header.part_index].empty()) {
            frame.parts[header.part_index].assign(buffer + 8, buffer + len);
            frame.received_parts++;
        }

        // Frame complete
        if (frame.received_parts == frame.total_parts) {
            std::vector<uint8_t> jpeg_data;
            for (const auto& part : frame.parts) {
                jpeg_data.insert(jpeg_data.end(), part.begin(), part.end());
            }

            // Decompress JPEG
            jpeg_decompress_struct cinfo;
            jpeg_error_mgr jerr;
            cinfo.err = jpeg_std_error(&jerr);
            jpeg_create_decompress(&cinfo);

            jpeg_mem_src(&cinfo, jpeg_data.data(), jpeg_data.size());
            jpeg_read_header(&cinfo, TRUE);
            jpeg_start_decompress(&cinfo);

            int width = cinfo.output_width;
            int height = cinfo.output_height;
            int channels = cinfo.output_components;

            std::vector<uint8_t> rgb_buf(width * height * channels);
            while (cinfo.output_scanline < cinfo.output_height) {
                uint8_t* rowptr = &rgb_buf[cinfo.output_scanline * width * channels];
                jpeg_read_scanlines(&cinfo, &rowptr, 1);
            }
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);

            // Display via SDL
            if (!texture) {
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
            }

            SDL_UpdateTexture(texture, nullptr, rgb_buf.data(), width * channels);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);

            frame_map.erase(header.frame_id);
        }

        // Cleanup stale frames
        uint64_t now = current_time_ms();
        for (auto it = frame_map.begin(); it != frame_map.end();) {
            if (now - it->second.last_update_time > FRAME_TIMEOUT_MS) {
                it = frame_map.erase(it);
            } else {
                ++it;
            }
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto quit;
        }
    }

quit:
    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close(sockfd);
    return 0;
}


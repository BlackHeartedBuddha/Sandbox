#include <iostream>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <opencv2/opencv.hpp>
#include <SDL2/SDL.h>

int main() {
    // Setup socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Change as needed

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }
    std::cout << "Connected to server\n";

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Video Stream",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          640, 480,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL Window could not be created: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL Renderer could not be created: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;
    int frame_width = 640;
    int frame_height = 480;

    while (running) {
        // Handle SDL events (e.g., window close)
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        // Receive size of incoming frame (network byte order)
        uint32_t size_net;
        ssize_t ret = recv(sockfd, &size_net, sizeof(size_net), MSG_WAITALL);
        if (ret <= 0) break;
        uint32_t size = ntohl(size_net);
        if (size == 0) break;

        // Receive frame data
        std::vector<uchar> buffer(size);
        size_t received = 0;
        while (received < size) {
            ret = recv(sockfd, buffer.data() + received, size - received, 0);
            if (ret <= 0) break;
            received += ret;
        }
        if (received < size) break;

        // Decode JPEG to OpenCV Mat (BGR)
        cv::Mat frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (frame.empty()) {
            std::cerr << "Failed to decode frame\n";
            continue;
        }

        // Convert to SDL texture format (RGB)
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

        // Create SDL surface from Mat data
        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
            frame.data, frame.cols, frame.rows, 24, frame.step,
            0x000000ff, 0x0000ff00, 0x00ff0000, 0);

        if (!surface) {
            std::cerr << "SDL_CreateRGBSurfaceFrom failed: " << SDL_GetError() << "\n";
            continue;
        }

        // Create texture from surface
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);

        if (!texture) {
            std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << "\n";
            continue;
        }

        // Clear renderer and draw texture
        SDL_RenderClear(renderer);

        // Get window size for scaling
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);

        SDL_Rect dstrect = { 0, 0, win_w, win_h };
        SDL_RenderCopy(renderer, texture, nullptr, &dstrect);
        SDL_RenderPresent(renderer);

        SDL_DestroyTexture(texture);
    }

    close(sockfd);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}


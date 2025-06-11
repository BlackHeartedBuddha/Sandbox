#include <iostream>
#include <vector>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // change if server IP differs

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }

    std::cout << "Connected to server\n";

    while (true) {
        uint32_t size_net;
        ssize_t ret = recv(sockfd, &size_net, sizeof(size_net), MSG_WAITALL);
        if (ret <= 0) break;

        uint32_t size = ntohl(size_net);
        if (size == 0) break;

        std::vector<uchar> buffer(size);
        size_t received = 0;
        while (received < size) {
            ret = recv(sockfd, buffer.data() + received, size - received, 0);
            if (ret <= 0) break;
            received += ret;
        }
        if (received < size) break;

        cv::Mat frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (frame.empty()) {
            std::cerr << "Failed to decode frame\n";
            break;
        }

        cv::imshow("Video Stream", frame);
        if (cv::waitKey(1) == 27) // ESC to quit
            break;
    }

    close(sockfd);
    return 0;
}


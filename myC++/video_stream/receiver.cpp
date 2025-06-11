#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr = {AF_INET, htons(8080)};
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    while (true) {
        int size = 0;
        recv(sock, &size, sizeof(size), MSG_WAITALL);  // Read size

        std::vector<uchar> buf(size);
        recv(sock, buf.data(), size, MSG_WAITALL);     // Read frame

        cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
        if (!img.empty()) {
            cv::imshow("Client", img);
            if (cv::waitKey(1) == 27) break;
        }
    }

    close(sock);
    return 0;
}


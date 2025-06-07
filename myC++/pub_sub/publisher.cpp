#include <zmq.hpp>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    zmq::context_t context(1);
    zmq::socket_t publisher(context, zmq::socket_type::pub);
    publisher.bind("tcp://*:5555");

    std::this_thread::sleep_for(std::chrono::seconds(1)); // Let subscribers connect

    int counter = 0;
    while (true) {
        std::string topic = "weather";
        std::string message = "Temperature is " + std::to_string(20 + counter % 10) + "°C";

        zmq::message_t topic_msg(topic.begin(), topic.end());
        zmq::message_t message_msg(message.begin(), message.end());

        publisher.send(topic_msg, zmq::send_flags::sndmore);
        publisher.send(message_msg, zmq::send_flags::none);

        std::cout << "[Publisher] " << topic << ": " << message << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
        counter++;
    }
}


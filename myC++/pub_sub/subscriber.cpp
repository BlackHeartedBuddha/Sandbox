#include <zmq.hpp>
#include <string>
#include <iostream>
#include <fstream>


int main() {
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    subscriber.connect("tcp://localhost:5555");

    std::string topic_filter = "weather";
    subscriber.set(zmq::sockopt::subscribe, topic_filter);

    // writing to local file to render in browser
    std::ofstream out("public/data.txt", std::ios::app);
    while (true) {
        zmq::message_t topic_msg;
        zmq::message_t message_msg;

        subscriber.recv(topic_msg, zmq::recv_flags::none);
        subscriber.recv(message_msg, zmq::recv_flags::none);

        std::string topic(static_cast<char*>(topic_msg.data()), topic_msg.size());
        std::string message(static_cast<char*>(message_msg.data()), message_msg.size());

        std::cout << "[Subscriber] " << topic << ": " << message << std::endl;
        out << topic << ": " << message << "\n";
        out.flush();
    }

    out.close();
}


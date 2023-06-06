#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

const int BUFFER_SIZE = 65536;  // Buffer size for UDP packets

using boost::asio::ip::udp;

std::vector<std::string> videoList = {
    "drone.mp4",
};

void sendVideoList(udp::socket& socket, const udp::endpoint& clientEndpoint) {
    std::ostringstream videoListStream;

    for (int i = 0; i < videoList.size(); ++i) {
        videoListStream << i << ": " << videoList[i] << "\n";
    }

    std::string videoListStr = videoListStream.str();
    socket.send_to(boost::asio::buffer(videoListStr), clientEndpoint);
}

void handleClientRequest(udp::socket& socket, const udp::endpoint& clientEndpoint, const std::string& videoNumber) {
    int videoIndex;
    try {
        videoIndex = boost::lexical_cast<int>(videoNumber);
    } catch (const boost::bad_lexical_cast&) {
        std::cerr << "Invalid video number: " << videoNumber << std::endl;
        return;
    }

    if (videoIndex < 0 || videoIndex >= videoList.size()) {
        std::cerr << "Invalid video number: " << videoNumber << std::endl;
        return;
    }

    std::string videoFile = videoList[videoIndex];
    std::ifstream file(videoFile, std::ios::binary);

    if (!file) {
        std::cerr << "Failed to open video file: " << videoFile << std::endl;
        return;
    }

    char buffer[BUFFER_SIZE];
    std::memset(buffer, 0, BUFFER_SIZE);

    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        socket.send_to(boost::asio::buffer(buffer, file.gcount()), clientEndpoint);
        std::memset(buffer, 0, BUFFER_SIZE);
    }

    file.close();
}

void handleClient(udp::socket& socket, udp::endpoint clientEndpoint) {

    char buffer[BUFFER_SIZE];
    std::memset(buffer, 0, BUFFER_SIZE);
    size_t receivedSize = socket.receive_from(boost::asio::buffer(buffer, BUFFER_SIZE), clientEndpoint);

    std::string request(buffer, receivedSize);
    boost::algorithm::trim(request);

    if (!request.empty()) {
        std::vector<std::string> requestParts;
        boost::split(requestParts, request, boost::is_any_of(" "));
        if (requestParts.size() == 2 && requestParts[0] == "SELECT") {
            std::string videoNumber = requestParts[1];
            handleClientRequest(socket, clientEndpoint, videoNumber);
        } else {
            std::cerr << "Invalid request: " << request << std::endl;
        }
    }
}

int main() {
    boost::asio::io_service ioService;
    udp::socket socket(ioService, udp::endpoint(udp::v4(), 5656));

    while (true) {
        udp::endpoint clientEndpoint;
        handleClient(socket, clientEndpoint);
        sendVideoList(socket, clientEndpoint);
    }

    return 0;
}

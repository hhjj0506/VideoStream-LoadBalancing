#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <opencv2/opencv.hpp>

const int BUFFER_SIZE = 65536;  // Buffer size for UDP packets

using boost::asio::ip::udp;

int main() {
    boost::asio::io_service ioService;
    udp::socket socket(ioService, udp::endpoint(udp::v4(), 0));

    udp::endpoint serverEndpoint(boost::asio::ip::address::from_string("127.0.0.1"), 5656);

    char buffer[BUFFER_SIZE];
    std::memset(buffer, 0, BUFFER_SIZE);
    socket.receive_from(boost::asio::buffer(buffer, BUFFER_SIZE), serverEndpoint);

    std::cout << "Available videos:" << std::endl;
    std::cout << buffer << std::endl;

    std::cout << "Select a video by entering its number:" << std::endl;

    int selectedVideo;
    std::cin >> selectedVideo;

    std::ostringstream requestStream;
    requestStream << "SELECT " << selectedVideo;
    std::string request = requestStream.str();

    socket.send_to(boost::asio::buffer(request), serverEndpoint);

    cv::namedWindow("Received Video", cv::WINDOW_NORMAL);

    while (true) {
        boost::system::error_code error;
        size_t receivedSize = socket.receive_from(boost::asio::buffer(buffer, BUFFER_SIZE), serverEndpoint, 0, error);

        if (error == boost::asio::error::eof) {
            break;  // End of the video stream
        } else if (error) {
            std::cerr << "Error receiving UDP packet: " << error.message() << std::endl;
            continue;
        }

        cv::Mat receivedFrame = cv::imdecode(cv::Mat(1, receivedSize, CV_8UC1, buffer), cv::IMREAD_COLOR);

        if (receivedFrame.empty()) {
            std::cerr << "Failed to decode frame" << std::endl;
            continue;
        }

        cv::imshow("Received Video", receivedFrame);

        if (cv::waitKey(1) >= 0)
            break;
    }

    socket.close();

    return 0;
}

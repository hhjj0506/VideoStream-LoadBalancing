#include "opencv2/opencv.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace cv;

int main(int argc, char **argv)
{
    // Networking stuff: socket, connect
    int sokt;
    char *serverIP;
    int serverPort;

    if (argc < 3)
    {
        std::cerr << "Usage: cv_video_cli <serverIP> <serverPort>" << std::endl;
        exit(1);
    }

    serverIP = argv[1];
    serverPort = atoi(argv[2]);

    struct sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(struct sockaddr_in);

    if ((sokt = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        std::cerr << "socket() failed" << std::endl;
        exit(1);
    }

    serverAddr.sin_family = PF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    serverAddr.sin_port = htons(serverPort);

    if (connect(sokt, (sockaddr *)&serverAddr, addrLen) < 0)
    {
        std::cerr << "connect() failed!" << std::endl;
        exit(1);
    }

    Mat img;
    int imgSize;
    int bytes = 0;
    int key;

    namedWindow("CV Video Client", 1);

    while (key != 'q')
    {
        // Receive the image data
        imgSize = 720 * 1280 * sizeof(uchar);
        img = Mat(720, 1280, CV_8UC1);

        if ((bytes = recv(sokt, img.data, imgSize, MSG_WAITALL)) == -1)
        {
            std::cerr << "recv failed, received bytes = " << bytes << std::endl;
            break;
        }

        // Display the received image
        cv::imshow("CV Video Client", img);

        if (key = cv::waitKey(10) >= 0)
            break;
    }

    close(sokt);

    return 0;
}

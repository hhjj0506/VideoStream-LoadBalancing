#include "opencv2/opencv.hpp"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>

using namespace cv;

void *display(void *);

std::string videoPath = "drone.mp4";  // Replace with the actual path to your MP4 video file
int capDev = 0;

int main(int argc, char **argv)
{
    // Networking stuff: socket, bind, listen
    int localSocket, remoteSocket, port = 5656;
    struct sockaddr_in localAddr, remoteAddr;
    pthread_t thread_id;
    int addrLen = sizeof(struct sockaddr_in);

    if ((argc > 1) && (strcmp(argv[1], "-h") == 0))
    {
        std::cerr << "usage: ./cv_video_srv [port] [capture device]\n"
                  << "port           : socket port (4097 default)\n"
                  << "capture device : (0 default)\n"
                  << std::endl;

        exit(1);
    }

    if (argc == 2)
        port = atoi(argv[1]);

    localSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (localSocket == -1)
    {
        perror("socket() call failed!!");
    }

    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(port);

    if (bind(localSocket, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0)
    {
        perror("Can't bind() socket");
        exit(1);
    }

    // Listening
    listen(localSocket, 3);

    std::cout << "Waiting for connections...\n"
              << "Server Port: " << port << std::endl;

    // Accept connection from an incoming client
    while (1)
    {
        remoteSocket = accept(localSocket, (struct sockaddr *)&remoteAddr, (socklen_t *)&addrLen);
        if (remoteSocket < 0)
        {
            perror("accept failed!");
            exit(1);
        }
        std::cout << "Connection accepted" << std::endl;
        pthread_create(&thread_id, NULL, display, &remoteSocket);
    }

    return 0;
}

void *display(void *ptr)
{
    int socket = *(int *)ptr;

    // OpenCV Code
    VideoCapture cap(videoPath); // Open the MP4 video file

    Mat img, imgGray;
    int imgSize;
    int bytes = 0;
    int key;

    while (1)
    {
        // Get a frame from the video
        if (!cap.read(img))
        {
            std::cerr << "Failed to read frame from video" << std::endl;
            break;
        }

        // Convert the frame to grayscale
        cvtColor(img, imgGray, COLOR_BGR2GRAY);

        // Prepare the image data to be sent
        imgSize = imgGray.total() * imgGray.elemSize();

        // Send the processed image
        if ((bytes = send(socket, imgGray.data, imgSize, 0)) < 0)
        {
            std::cerr << "bytes = " << bytes << std::endl;
            break;
        }
    }
}

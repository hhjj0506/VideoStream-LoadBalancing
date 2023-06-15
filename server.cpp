#include "opencv2/opencv.hpp"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <chrono>

using namespace cv;
using namespace std;

void *handleClient(void *);

vector<string> videoList = {
    "drone.mp4",
    "test.mp4",
    "drone.mp4"
};

const string DELIMITER = "\n"; // Delimiter to separate video names

int main(int argc, char** argv)
{
    //--------------------------------------------------------
    // Networking stuff: socket, bind, listen
    //--------------------------------------------------------
    int localSocket, remoteSocket, port = 4097;

    struct sockaddr_in localAddr, remoteAddr;
    pthread_t thread_id;

    int addrLen = sizeof(struct sockaddr_in);

    if ((argc > 1) && (strcmp(argv[1], "-h") == 0)) {
        cerr << "usage: ./cv_video_srv [port] [capture device]\n"
                  << "port           : socket port (4097 default)\n"
                  << "capture device : (0 default)\n"
                  << endl;

        exit(1);
    }

    if (argc == 2)
        port = atoi(argv[1]);

    localSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (localSocket == -1) {
        perror("socket() call failed!!");
        exit(1);
    }

    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(port);

    if (::bind(localSocket, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        perror("Can't bind() socket");
        exit(1);
    }

    // Listening
    listen(localSocket, 3);

    cout << "Waiting for connections..." << endl;
    cout << "Server Port: " << port << endl;

    while (1) {
        remoteSocket = accept(localSocket, (struct sockaddr*)&remoteAddr, (socklen_t*)&addrLen);
        if (remoteSocket < 0) {
            perror("accept failed!");
            exit(1);
        }

        cout << "Connection accepted" << endl;
        pthread_create(&thread_id, NULL, handleClient, (void*)&remoteSocket);
        pthread_detach(thread_id);
    }

    return 0;
}

void *handleClient(void* socketPtr)
{
    int socket = *(int*)socketPtr;

    // Send video list size to the client
    int videoListSize = 0;
    for (const auto& video : videoList) {
        videoListSize += video.length() + DELIMITER.length();
    }
    if (send(socket, &videoListSize, sizeof(videoListSize), 0) < 0) {
        cerr << "Failed to send video list size" << endl;
        close(socket);
        pthread_exit(NULL);
    }

    //--------------------------------------------------------
    // Send video list to the client
    //--------------------------------------------------------
    for (const auto& video : videoList) {
        string videoNameWithDelimiter = video + DELIMITER;
        if (send(socket, videoNameWithDelimiter.c_str(), videoNameWithDelimiter.length(), 0) < 0) {
            cerr << "Failed to send video name" << endl;
            break;
        }
    }

    //--------------------------------------------------------
    // Receive chosen video request from the client
    //--------------------------------------------------------
    char chosenVideo[256];
    int bytesReceived = recv(socket, chosenVideo, sizeof(chosenVideo), 0);
    if (bytesReceived <= 0) {
        cerr << "Failed to receive chosen video request" << endl;
        close(socket);
        pthread_exit(NULL);
    }

    //--------------------------------------------------------
    // Open the chosen video
    //--------------------------------------------------------
    string chosenVideoName(chosenVideo, bytesReceived);
    VideoCapture cap(chosenVideoName);
    if (!cap.isOpened()) {
        cerr << "Failed to open chosen video: " << chosenVideoName << endl;
        close(socket);
        pthread_exit(NULL);
    }

    //--------------------------------------------------------
    // Send video frames to the client
    //--------------------------------------------------------
    Mat img, imgGray;
    int imgSize;
    int bytes = 0;
    int key;
    int check = 1;
    vector<float> speedList;
    float ave = 0.0;
    float networkSpeed = 0.0;
    int i = 0;

    while (1)
    {
        // Get a frame from the video
        if (!cap.read(img))
        {
            cerr << "Failed to read frame from video" << endl;
            break;
        }

        // Convert the frame to grayscale
        cvtColor(img, imgGray, COLOR_BGR2GRAY);
        if (ave != 0.0 && networkSpeed < ave) {
            GaussianBlur(imgGray, imgGray, Size(15, 15), 0);
        }

        // Prepare the image data to be sent
        imgSize = imgGray.total() * imgGray.elemSize();

        cout << imgSize << endl;

        // Send the processed image
        auto start = chrono::high_resolution_clock::now();
        if ((bytes = send(socket, imgGray.data, imgSize, 0)) < 0)
        {
            std::cerr << "bytes = " << bytes << endl;
            break;
        }

        char ack;
        if (recv(socket, &ack, sizeof(ack), 0) < 0) {
            cerr << "Failed to receive acknowledgement" << endl;
        }
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        networkSpeed = ((float)imgSize / (1024.0 * 1024.0)) / (duration / 1000.0);
        if (isinf(networkSpeed)) {
            networkSpeed = 0.0; // Set a default value
        }

        if (i < 30) {
            speedList.push_back(networkSpeed);
            i++;
        }

        if (i >= 29 && check == 1) {
            float sum = 0.0f;
            for (float num : speedList) {
                sum += num;
            }
            ave = sum / speedList.size();
            check = 0;
        }

        cout << "Network speed: " << networkSpeed << " Mbps" << endl;
    }

    //--------------------------------------------------------
    // Cleanup and close client connection
    //--------------------------------------------------------
    cap.release();
    close(socket);
    pthread_exit(NULL);
}
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
void color_depth_reduction(Mat &image, int num);

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

    localSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (localSocket == -1) {
        perror("socket() call failed!!");
        exit(1);
    }

    localAddr.sin_family = PF_INET;
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
            continue; // Skip to the next iteration of the loop
        }

        cout << "Connection accepted" << endl;
        if(pthread_create(&thread_id, NULL, handleClient, (void*)&remoteSocket) < 0) {
            perror("pthread_create failed");
            continue; // Skip to the next iteration of the loop
        };
        pthread_detach(thread_id);
    }

    close(localSocket);
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
    int blurDuration = 0;
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
            //GaussianBlur(imgGray, imgGray, Size(15, 15), 0);
            color_depth_reduction(imgGray, 64);
            blurDuration = 5;
        } else if (blurDuration > 0) {
            //color_depth_reduction(imgGray, 64);
            //GaussianBlur(imgGray, imgGray, Size(15, 15), 0);
            blurDuration--;
        }

        // Prepare the image data to be sent
        imgSize = imgGray.total() * imgGray.elemSize();

        // Send the processed image
        auto start = chrono::high_resolution_clock::now();
        if ((bytes = send(socket, imgGray.data, imgSize, 0)) < 0)
        {
            cerr << "bytes = " << bytes << endl;
            break;
        }

        char ack;
        if (recv(socket, &ack, sizeof(ack), 0) < 0) {
            cerr << "Failed to receive acknowledgement" << endl;
            break;
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

        cout << "Network speed: " << networkSpeed << " MB/s" << endl;
    }

    // Release the video capture and close the socket
    cap.release();
    close(socket);
    pthread_exit(NULL);
}

void color_depth_reduction(Mat &image, int num) {
    int h = image.rows;
    int w = image.cols;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (image.channels() == 1) {
                uchar data = image.at<uchar>(y, x);
                data = data/num*num + num/2;
                image.at<uchar>(y,x) = data;
            }
            else if (image.channels() == 3) {
                Vec3b colVal = image.at<Vec3b>(y, x);
                colVal[2] = colVal[2]/num*num + num/2;
                colVal[1] = colVal[1]/num*num + num/2;
                colVal[0] = colVal[0]/num*num + num/2;
                image.at<Vec3b>(y,x) = colVal;
            }
        }
    }
}
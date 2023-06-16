#include "opencv2/opencv.hpp"
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sstream>

using namespace cv;
using namespace std;

int main(int argc, char** argv)
{
    //--------------------------------------------------------
    // Networking stuff: socket, connect
    //--------------------------------------------------------
    int sock;
    char* serverIP;
    int serverPort;

    if (argc < 3) {
        cerr << "Usage: client <serverIP> <serverPort>" << endl;
        return 1;
    }

    serverIP = argv[1];
    serverPort = atoi(argv[2]);

    struct sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(struct sockaddr_in);

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "socket() failed" << endl;
        return 1;
    }

    serverAddr.sin_family = PF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    serverAddr.sin_port = htons(serverPort);

    if (connect(sock, (sockaddr*)&serverAddr, addrLen) < 0) {
        cerr << "connect() failed!" << endl;
        return 1;
    }

    //--------------------------------------------------------
    // Receive video list size from server
    //--------------------------------------------------------
    int videoListSize;
    if (recv(sock, &videoListSize, sizeof(videoListSize), 0) < 0) {
        cerr << "Failed to receive video list size" << endl;
        close(sock);
        return 1;
    }

    //--------------------------------------------------------
    // Receive video list from server
    //--------------------------------------------------------
    vector<string> videoList;
    char buffer[1024];
    int bytesRead = 0;

    while (bytesRead < videoListSize) {
        int bytesReceived = recv(sock, buffer + bytesRead, videoListSize - bytesRead, 0);
        if (bytesReceived <= 0) {
            cerr << "Failed to receive video list" << endl;
            break;
        }
        bytesRead += bytesReceived;
    }
    buffer[bytesRead] = '\0'; // Add null terminator to the buffer

    stringstream ss(string(buffer, bytesRead));
    string line;
    string videoName;
    while (getline(ss, line)) {
        if (!line.empty()) {
            videoList.push_back(line);
        }
    }

    if (videoList.empty()) {
        cerr << "No videos available" << endl;
        close(sock);
        return 1;
    }

    //--------------------------------------------------------
    // Prompt user to choose a video
    //--------------------------------------------------------
    cout << "Available videos:" << endl;
    for (int i = 0; i < videoList.size(); ++i) {
        cout << i + 1 << ". " << videoList[i] << endl;
    }

    int chosenVideo;
    cout << "Enter the number of the video you want to play: ";
    cin >> chosenVideo;

    if (chosenVideo < 1 || chosenVideo > videoList.size()) {
        cerr << "Invalid video number" << endl;
        close(sock);
        return 1;
    }

    //--------------------------------------------------------
    // Request the chosen video from the server
    //--------------------------------------------------------
    string chosenVideoName = videoList[chosenVideo - 1];
    cout << chosenVideoName << endl;
    if (send(sock, chosenVideoName.c_str(), chosenVideoName.length(), 0) < 0) {
        cerr << "Failed to send chosen video request" << endl;
        close(sock);
        return 1;
    }

    //--------------------------------------------------------
    // OpenCV Code to display the video
    //--------------------------------------------------------
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

        if ((bytes = recv(sock, img.data, imgSize, MSG_WAITALL)) == -1)
        {
            cerr << "recv failed, received bytes = " << bytes << endl;
            break;
        }

        // Display the received image
        cv::imshow("CV Video Client", img);

        char ack = 'A';
        if (send(sock, &ack, sizeof(ack), 0) < 0) {
            cerr << "Failed to send acknowledgement" << endl;
            close(sock);
            return 1;
        }

        if (key = cv::waitKey(10) >= 0)
            break;
    }

    close(sock);

    return 0;

}
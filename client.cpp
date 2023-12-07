#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

#include "CongestionController.h"
#include "utils.h"

void packageFile(std::ifstream &fp, std::vector<packet> &packetVec) {
    char buff[1024];
    unsigned short i = 0;

    fp.seekg(0, std::ios::end);
    long length = fp.tellg();
    fp.seekg(0, std::ios::beg);

    while (fp.tellg() != length) {
        packet pack;
        int toRead = std::min(int(length - fp.tellg()), 1024);
        fp.read(buff, toRead);
        build_packet(&pack, i, 0, 0, 0, toRead, buff);
        packetVec.push_back(pack);
        i++;
    }
    packetVec[packetVec.size() - 1].last = 1;
}

struct pendingPacketNode {
    // packNum -> index in sender packetList
    int packNum;
    timeval sentTime;

    pendingPacketNode(int num, timeval tv, pendingPacketNode *p = nullptr)
        : packNum(num), sentTime(tv) {}
};

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10;

    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   sizeof timeout) < 0) {
        perror("setsockopt failed\n");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr,
             sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    std::ifstream fp(filename);
    if (!fp.is_open()) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // RDT parameters
    // const long long kTimeoutMicro = 250000;
    const long long kTimeoutMicro = 2500000;
    // RDT sender state
    int numDupes = 0;
    int cwnd = 1;
    int ssthresh = 4;
    std::vector<packet> packetList;
    packageFile(fp, packetList);

    // congestion control
    CongestionController congestionController(ssthresh);

    timeval sentTime;
    if (gettimeofday(&sentTime, nullptr) != 0) {
        std::cerr << "Error getting the current time" << std::endl;
        return -1;
    }
    std::queue<pendingPacketNode> pendingQueue;
    pendingQueue.push(pendingPacketNode(0, sentTime));

    sendPacket(send_sockfd, &server_addr_to, packetList[0]);
    while (true) {
        auto [recvPack, isPresent] = readPacket(listen_sockfd);

        // check for timeouts
        if (!isPresent) {
            timeval currTime;
            if (gettimeofday(&currTime, nullptr) != 0) {
                std::cerr << "Error getting the current time" << std::endl;
                return -1;
            }
            long long timeElapsedMicro =
                getTimeElapsed(pendingQueue.front().sentTime, currTime);

            if (timeElapsedMicro >= kTimeoutMicro) {
                pendingQueue.front().sentTime = currTime;
                std::cout << "timeout: " << pendingQueue.front().packNum
                          << std::endl;
                sendPacket(send_sockfd, &server_addr_to,
                           packetList[pendingQueue.front().packNum]);
                numDupes = 0;
                cwnd = congestionController.gotTimeout();
            }
        } else {
            if (recvPack.last == 1) break;  // do handshake stuff
            int recvAck = recvPack.acknum;
            if (recvAck == pendingQueue.front().packNum) {
                cwnd = congestionController.gotDupAck();
                numDupes += 1;
                std::cout << "numDupes: " << numDupes << std::endl;
                if (numDupes == 3) {
                    std::cout
                        << "fast retransmit: " << pendingQueue.front().packNum
                        << std::endl;
                    sendPacket(send_sockfd, &server_addr_to,
                               packetList[pendingQueue.front().packNum]);
                }
            } else if (recvAck > pendingQueue.front().packNum) {
                int initialCwnd = cwnd;
                cwnd = congestionController.gotNewAck();
                numDupes = 0;
                int initialPackNum = pendingQueue.front().packNum;
                for (int i = initialPackNum + initialCwnd; i < recvAck + cwnd;
                     ++i) {
                    if (i >= packetList.size()) {
                        break;
                    }
                    std::cout << "sending packet: " << i << std::endl;
                    sendPacket(send_sockfd, &server_addr_to, packetList[i]);
                    timeval sentTime;
                    if (gettimeofday(&sentTime, nullptr) != 0) {
                        std::cerr << "Error getting the current time"
                                  << std::endl;
                        return -1;
                    }
                    pendingQueue.push(pendingPacketNode(i, sentTime));
                    usleep(10000);
                }
                for (int i = initialPackNum; i < recvAck; ++i) {
                    pendingQueue.pop();
                }
            }
        }
    }
    fp.close();
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

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
    char buff[1190];
    unsigned short i = 0;

    fp.seekg(0, std::ios::end);
    long length = fp.tellg();
    fp.seekg(0, std::ios::beg);

    while (fp.tellg() != length) {
        packet pack;
        int toRead = std::min(int(length - fp.tellg()), 1190);
        fp.read(buff, toRead);
        build_packet(&pack, i, 0, 0, 0, toRead, buff);
        packetVec.push_back(pack);
        i++;
    }
}

struct pendingPacketNode {
    // packNum -> index in sender packetList
    int packNum;
    timeval sentTime;

    pendingPacketNode(int num, timeval tv) : packNum(num), sentTime(tv) {}
};

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to;

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
    const long long kTimeoutMicro = 0.8 * 1000000;
    // RDT sender state
    int numDupes = 0;
    int cwnd = 1;
    int ssthresh = 4;
    std::vector<packet> packetList;
    packageFile(fp, packetList);
    std::vector<bool> packetWasSent(packetList.size(), false);

    // congestion control
    CongestionController congestionController(ssthresh);

    timeval sentTime;
    if (gettimeofday(&sentTime, nullptr) != 0) {
        std::cerr << "Error getting the current time" << std::endl;
        return -1;
    }
    std::deque<pendingPacketNode> pendingQueue;
    pendingQueue.push_back(pendingPacketNode(0, sentTime));

    sendPacket(send_sockfd, &server_addr_to, packetList[0]);
    while (!pendingQueue.empty()) {
        packet recvPack;
        bool isPresent;
        std::tie(recvPack, isPresent) = readPacket(listen_sockfd);

        // check for timeouts
        if (!isPresent) {
            timeval currTime;
            if (gettimeofday(&currTime, nullptr) != 0) {
                std::cerr << "Error getting the current time" << std::endl;
                return -1;
            }
            for (auto &pendingPack : pendingQueue) {
                long long timeElapsedMicro =
                    getTimeElapsed(pendingPack.sentTime, currTime);

                if (timeElapsedMicro >= kTimeoutMicro) {
                    pendingPack.sentTime = currTime;
                    sendPacket(send_sockfd, &server_addr_to,
                               packetList[pendingPack.packNum]);
                    numDupes = 0;
                    cwnd = congestionController.gotTimeout();
                }
            }
        } else {
            int recvAck = recvPack.acknum;
            if (recvAck == pendingQueue.front().packNum) {
                cwnd = congestionController.gotDupAck();
                numDupes += 1;
                if (numDupes == 3) {
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
                    if (packetWasSent[i]) {
                        continue;
                    }
                    packetWasSent[i] = true;
                    sendPacket(send_sockfd, &server_addr_to, packetList[i]);
                    timeval sentTime;
                    if (gettimeofday(&sentTime, nullptr) != 0) {
                        std::cerr << "Error getting the current time"
                                  << std::endl;
                        return -1;
                    }
                    pendingQueue.push_back(pendingPacketNode(i, sentTime));
                    usleep(10000);
                }
                for (int i = initialPackNum; i < recvAck; ++i) {
                    pendingQueue.pop_front();
                }
            }
        }
    }

    sendPacket(send_sockfd, &server_addr_to,
               packet{uint16_t(packetList.size() % 65535), 1, 1, 1, 0, {}});
    if (gettimeofday(&sentTime, nullptr) != 0) {
        std::cerr << "Error getting the current time" << std::endl;
        return -1;
    }
    while (true) {
        packet recvPack;
        bool isPresent;
        std::tie(recvPack, isPresent) = readPacket(listen_sockfd);

        if (!isPresent) {
            timeval timeNow;
            if (gettimeofday(&timeNow, nullptr) != 0) {
                std::cerr << "Error getting the current time" << std::endl;
                return -1;
            }
            if (getTimeElapsed(sentTime, timeNow) > kTimeoutMicro) {
                sendPacket(
                    send_sockfd, &server_addr_to,
                    packet{
                        uint16_t(packetList.size() % 65535), 1, 1, 1, 0, {}});
                if (gettimeofday(&sentTime, nullptr) != 0) {
                    std::cerr << "Error getting the current time" << std::endl;
                    return -1;
                }
            }
            continue;
        }
        if (recvPack.last == 1) {
            break;
        }
    }
    fp.close();
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

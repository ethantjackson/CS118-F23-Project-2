#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <vector>

#include "utils.h"

void writeToFile(std::ofstream &fp, std::vector<packet> &packets) {
    for (packet p : packets) {
        // for (unsigned int i = 0; i < p.length; ++i) {
        //     fp << p.payload[i];
        // }
        fp.write(p.payload, p.length);
    }
}

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
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

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    std::vector<packet> packetList;
    // RTL state
    std::vector<bool> ackedPacks;
    unsigned short cumAck = 0;

    while (true) {
        packet recvPack;
        bool isPresent;
        std::tie(recvPack, isPresent) = readPacket(listen_sockfd);
        // packet recvPack = std::get<0>(retval);
        // bool isPresent = std::get<1>(retval);
        if (!isPresent) continue;
        printRecv(&recvPack);
        for (int i = ackedPacks.size(); i <= recvPack.seqnum; ++i) {
            packetList.push_back(packet{});
            ackedPacks.push_back(false);
        }
        packetList[recvPack.seqnum] = recvPack;
        ackedPacks[recvPack.seqnum] = true;
        while (cumAck < ackedPacks.size() && ackedPacks[cumAck]) {
            cumAck += 1;
        }
        if (recvPack.last == 1) {
            // initiate handshake
            sendPacket(send_sockfd, &client_addr_to,
                       packet{999, cumAck, 1, 1, 0});
            break;
        }
        sendPacket(send_sockfd, &client_addr_to, packet{999, cumAck, 1, 0, 0});
    }

    // Open the target file for writing (always write to output.txt)
    std::ofstream fp("output.txt", std::ios_base::binary);
    if (!fp.is_open()) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    std::cout << "writing to file\n";
    writeToFile(fp, packetList);

    fp.close();
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

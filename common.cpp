
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdexcept>
#include <string>

std::optional<packet> readPacket(int sockfd) {
    std::optional<packet> pack;

    unsigned int bytesRead = read(sockfd, &pack.value().seqnum, 2);

    if (bytesRead == 0)
        return std::nullopt;
    else if (bytesRead != 2)
        throw std::runtime_error("unable to read packet seqnum");

    if (read(sockfd, &pack.value().acknum, 2) != 2)
        throw std::runtime_error("unable to read packet acknum");
    if (read(sockfd, &pack.value().ack, 1) != 1)
        throw std::runtime_error("unable to read packet ack");
    if (read(sockfd, &pack.value().last, 1) != 1)
        throw std::runtime_error("unable to read packet last");
    if (read(sockfd, &pack.value().length, 4) != 4)
        throw std::runtime_error("unable to read packet length");

    pack.value().seqnum = ntohs(pack.value().seqnum);
    pack.value().acknum = ntohs(pack.value().acknum);
    pack.value().length = ntohl(pack.value().length);

    char* buff[1024];
    bytesRead = read(sockfd, buff, pack.value().length);
    if (bytesRead != pack.value().length)
        throw std::runtime_error("unable to read packet payload");

    memcpy(pack.value().payload, buff, pack.value().length);

    return pack;
}

bool sendPacket(int sockfd, const packet& pack) {
    short netseqnum = htons(pack.seqnum);
    short netacknum = htons(pack.acknum);
    unsigned int netlength = htonl(pack.length);

    if (send(sockfd, &netseqnum, 2, 0) != 2)
        throw std::runtime_error("unable to send packet seqnum");
    if (send(sockfd, &netacknum, 2, 0) != 2)
        throw std::runtime_error("unable to send packet acknum");
    if (send(sockfd, &pack.ack, 1, 0) != 1)
        throw std::runtime_error("unable to send packet ack");
    if (send(sockfd, &pack.last, 1, 0) != 1)
        throw std::runtime_error("unable to send packet last");
    if (send(sockfd, &netlength, 4, 0) != 4)
        throw std::runtime_error("unable to send packet length");
    if (send(sockfd, &pack.payload, pack.length, 0) != pack.length)
        throw std::runtime_error("unable to send packet payload");

    return true;
}

std::tuple<int, int> getControlWindow(int ssthresh, int currentCwnd,
                                      int numDups);

std::vector<packet> packageFile(int fp);

bool writeToFile(int fp, std::vector<packet> packets);
#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <tuple>

// MACROS
#define SERVER_IP "127.0.0.1"
#define LOCAL_HOST "127.0.0.1"
#define SERVER_PORT_TO 5002
#define CLIENT_PORT 6001
#define SERVER_PORT 6002
#define CLIENT_PORT_TO 5001
#define PAYLOAD_SIZE 1024
#define WINDOW_SIZE 5
#define TIMEOUT 2
#define MAX_SEQUENCE 1024

// Packet Layout
// You may change this if you want to
struct packet
{
    unsigned short seqnum;
    unsigned short acknum;
    char ack;
    char last;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Utility function to build a packet
void build_packet(struct packet *pkt, unsigned short seqnum,
                  unsigned short acknum, char last, char ack,
                  unsigned int length, const char *payload)
{
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->ack = ack;
    pkt->last = last;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// Utility function to print a packet
void printRecv(struct packet *pkt)
{
    printf("RECV %d %d%s%s\n", pkt->seqnum, pkt->acknum,
           pkt->last ? " LAST" : "", (pkt->ack) ? " ACK" : "");
}

void printSend(struct packet *pkt, int resend)
{
    if (resend)
        printf("RESEND %d %d%s%s\n", pkt->seqnum, pkt->acknum,
               pkt->last ? " LAST" : "", pkt->ack ? " ACK" : "");
    else
        printf("SEND %d %d%s%s\n", pkt->seqnum, pkt->acknum,
               pkt->last ? " LAST" : "", pkt->ack ? " ACK" : "");
}

bool readBytes(int sockfd, void *buf, int size, bool shouldTimeout = false)
{
    int totalBytesRead = 0;
    while (totalBytesRead < size)
    {
        int bytesRead =
            read(sockfd, (char *)buf + totalBytesRead, size - totalBytesRead);
        if (bytesRead == -1 && shouldTimeout)
            return false;

        totalBytesRead += std::max(0, bytesRead);
    }
    return true;
}

std::tuple<packet, bool> readPacket(int sockfd)
{
    packet pack;

    if (!readBytes(sockfd, &pack.seqnum, 2, true))
        return std::make_tuple(packet{}, false);
    readBytes(sockfd, &pack.acknum, 2);
    readBytes(sockfd, &pack.ack, 1);
    readBytes(sockfd, &pack.last, 1);
    readBytes(sockfd, &pack.length, 4);

    pack.seqnum = ntohs(pack.seqnum);
    pack.acknum = ntohs(pack.acknum);
    pack.length = ntohl(pack.length);

    readBytes(sockfd, pack.payload, pack.length);

    return std::make_tuple(pack, true);
}

bool sendPacket(const int sockfd, const sockaddr_in *toInfo,
                const packet &pack)
{
    short netseqnum = htons(pack.seqnum);
    short netacknum = htons(pack.acknum);
    unsigned int netlength = htonl(pack.length);

    if (sendto(sockfd, &netseqnum, 2, 0, (struct sockaddr *)toInfo,
               sizeof(*toInfo)) != 2)
    {
        throw std::runtime_error("unable to send packet seqnum");
    }

    if (sendto(sockfd, &netacknum, 2, 0, (struct sockaddr *)toInfo,
               sizeof(*toInfo)) != 2)
        throw std::runtime_error("unable to send packet acknum");

    if (sendto(sockfd, &pack.ack, 1, 0, (struct sockaddr *)toInfo,
               sizeof(*toInfo)) != 1)
        throw std::runtime_error("unable to send packet ack");

    if (sendto(sockfd, &pack.last, 1, 0, (struct sockaddr *)toInfo,
               sizeof(*toInfo)) != 1)
        throw std::runtime_error("unable to send packet last");

    if (sendto(sockfd, &netlength, 4, 0, (struct sockaddr *)toInfo,
               sizeof(*toInfo)) != 4)
        throw std::runtime_error("unable to send packet length");

    if (sendto(sockfd, &pack.payload, pack.length, 0, (struct sockaddr *)toInfo,
               sizeof(*toInfo)) != pack.length)
        throw std::runtime_error("unable to send packet payload");

    return true;
}

long long getTimeElapsed(const timeval &start, const timeval &end)
{
    return (end.tv_sec - start.tv_sec) * 1000000LL + (end.tv_usec - start.tv_usec);
}

#endif
#include "CongestionController.h"

#include <algorithm>

CongestionController::CongestionController(int ssthresh) : ssthresh(ssthresh) {}

int CongestionController::gotNewAck() {
    if (inRecovery) {
        cwnd = ssthresh;
        inRecovery = false;
        numDups = 0;
    } else {
        if (cwnd <= ssthresh)
            cwnd *= 2;
        else
            cwnd++;
    }
    return cwnd;
}

int CongestionController::gotDupAck() {
    numDups++;
    if (inRecovery) {
        cwnd++;
    } else if (numDups == 3) {
        cwnd = std::max(cwnd / 2, 1);
        ssthresh = cwnd;
    }

    return cwnd;
}

int CongestionController::gotTimeout() {
    ssthresh = std::max(cwnd / 2, 1);
    cwnd = 1;
    inRecovery = false;
    numDups = 0;

    return cwnd;
}
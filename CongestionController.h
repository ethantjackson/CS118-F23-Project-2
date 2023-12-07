#pragma once

class CongestionController {
    CongestionController(int ssthresh);
    int gotNewAck();
    int gotDupAck();
    int gotTimout();
    int getCwnd();

   private:
    int ssthresh;
    int cwnd = 1;
    int numDups = 0;
    bool inRecovery = false;
};
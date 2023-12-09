#pragma once

class CongestionController {
   public:
    CongestionController(int ssthresh);
    int gotNewAck();
    int gotDupAck();
    int gotTimeout();
    int getCwnd();

   private:
    int ssthresh;
    int cwnd = 1;
    int numDups = 0;
    bool inRecovery = false;
};
//
// Created by Karven on 2017/3/8.
//

#ifndef DEXDUMP_SIGCATCHER_H
#define DEXDUMP_SIGCATCHER_H


#include <functional>
#include <signal.h>
#include "../support/globals.h"

class SigCatcher {
    std::function<bool()> func;
    struct sigaction sa;
    struct sigaction osa;
    SigCatcher* oldCatcher= nullptr;
    SigCatcher()= delete;
    SigCatcher(SigCatcher& sigCatcher)= delete;
    SigCatcher(u1 sig,std::function<bool()> func);
    static void sigHandler(int sig,struct siginfo *, void *);
public:
    static void catchSig(u1 sig, std::function<bool()> func);
    ~SigCatcher();


};


#endif //DEXDUMP_SIGCATCHER_H

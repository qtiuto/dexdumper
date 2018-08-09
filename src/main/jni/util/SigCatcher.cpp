//
// Created by Karven on 2017/3/8.
//

#include <unordered_map>
#include "SigCatcher.h"
#include "MyLog.h"

typedef std::unordered_map<int,SigCatcher*> SigMap;
static SigMap* sigMap;

void SigCatcher::sigHandler(int sig,struct siginfo * info, void * extra){
    if(sigMap== nullptr){
        LOGE("Error Unexpected sigMap==null");
        exit(0);
    }
    SigMap::iterator iterator=sigMap->find(sig);
    if(iterator==sigMap->end())
        return;
    SigCatcher* sigCatcher=iterator->second;

    bool shouldContinue= true;
    do{
        if(shouldContinue){
            try {
                shouldContinue=sigCatcher->func();
            }catch (...){
                shouldContinue=false;
            }
        }
        if(sigCatcher->oldCatcher== nullptr){
            break;
        }
        sigCatcher=sigCatcher->oldCatcher;
    }while (true);
    if(shouldContinue){
        if((sigCatcher->osa.sa_flags&SA_SIGINFO)==0){
            sigCatcher->osa.sa_sigaction(sig,info,extra);
        } else{
            sigCatcher->osa.sa_handler(sig);
        }
    }
    sigaction(sig,&sigCatcher->osa,&sigCatcher->sa);
    sigMap->erase(iterator);
    delete sigCatcher;

}
SigCatcher::SigCatcher(u1 sig, std::function<bool()> func):func(func) {
    sa.sa_sigaction=sigHandler;
    memset(&sa.sa_mask, 0, sizeof(sa.sa_mask));
    sa.sa_flags=SA_SIGINFO;
    sigaction(sig,&sa,&osa);
}

void SigCatcher::catchSig(u1 sig, std::function<bool()> func) {
    SigCatcher* sigCatcher=new SigCatcher(sig,func);
    if(sigMap== nullptr){
        sigMap=new SigMap();
    }

    /*SigMap::iterator iterator=sigMap->find((int)sig);
    if(iterator!= sigMap->end()){
        SigCatcher* oldCatcher=iterator->second;
        sigCatcher->oldCatcher=oldCatcher;
    }*/
    sigMap->emplace(sig,sigCatcher);
}

SigCatcher::~SigCatcher() {
    if( sigMap!= nullptr&&sigMap->size()==0){
        delete sigMap;
        sigMap= nullptr;
    }
    if(oldCatcher!= nullptr) delete oldCatcher;
}
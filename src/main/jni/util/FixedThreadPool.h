//
// Created by asus on 2016/8/21.
//

#ifndef HOOKMANAGER_FIXEDTHREADPOOL_H
#define HOOKMANAGER_FIXEDTHREADPOOL_H

#include <time.h>
#include <sys/time.h>
#include <atomic>
#include "../Tools.h"

class FixedThreadPool {
    struct Element {
        void* args= nullptr;
        volatile Element * nextElement= nullptr;

        Element(void *args, volatile Element *nextElement) :
                args(args), nextElement(nextElement) { }
    };
    volatile Element* pollNewTask();
    static void* threadFunc(void* args);
    //static void atThreadCrash(int sig,siginfo_t* siginfo,void* context);
    void triggerNewThread() ;
    const int poolSize;
    timespec realTime;
    const long waitTime;
    void* ( *const run)(void* args);
    void (*onInit)();
    void (*onDestroy)();
    volatile std::atomic_bool isFinishing{false};
    volatile std::atomic<unsigned int> runningThreads{0};
    volatile unsigned int pendingTasks{0};
    volatile unsigned int waitingThreads{0};
    volatile Element* curElement= nullptr;
    pthread_cond_t waitCond=PTHREAD_COND_INITIALIZER;
    pthread_mutex_t queueMutex=PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t queueCond=PTHREAD_COND_INITIALIZER;
public:
    FixedThreadPool(int poolSize,long waitTime_,void* (* run_)(void* args),
                    void (*onInit_)()= nullptr, void (*onDestroy_)()= nullptr):
            poolSize(poolSize),waitTime(waitTime_),run(run_),onInit(onInit_),onDestroy(onDestroy_){
    }

    void submit(void*args= nullptr);
    void execute(void* args= nullptr);
    void executeAllPendingTasks();
    bool isDirty(){
        return poolSize!=1;
    }
    void waitForFinish(){
        isFinishing=true;
        pthread_mutex_lock(&queueMutex);
        if(runningThreads.load()>0){
            pthread_cond_wait(&queueCond,&queueMutex);
        }
        pthread_mutex_unlock(&queueMutex);
    }
    void reOpen(){
        if(!isFinishing){
            LOGW("The pool hasn't been closed");
        }
        isFinishing= false;
    }
    void shutDown(){
        isFinishing= true;
        pthread_mutex_lock(&queueMutex);
        if(waitingThreads>0){
            pthread_cond_broadcast(&waitCond);
        }
        pthread_mutex_unlock(&queueMutex);
    }
    ~FixedThreadPool(){
        delete curElement;
    };

};


#endif //HOOKMANAGER_FIXEDTHREADPOOL_H

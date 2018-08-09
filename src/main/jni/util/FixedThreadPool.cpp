//
// Created by asus on 2016/8/21.
//

#include "FixedThreadPool.h"

void FixedThreadPool::execute( void *args) {
    pthread_mutex_lock(&queueMutex);
    if(isFinishing){
        pthread_mutex_unlock(&queueMutex);
        return;
    }
    curElement=new Element(args,curElement);
    ++pendingTasks;
    if(waitingThreads>0){
        pthread_cond_signal(&waitCond);
    } else if(runningThreads<poolSize){
        triggerNewThread();
    }
    pthread_mutex_unlock(&queueMutex);
}

void FixedThreadPool::triggerNewThread()  {
    ++runningThreads;
    pthread_t tid;
    pthread_create(&tid, nullptr,threadFunc,this);
    pthread_detach(tid);
}

void FixedThreadPool::executeAllPendingTasks() {
    //pthread_mutex_lock(&queueMutex);
    int ret=pthread_mutex_trylock(&queueMutex);
    if(waitingThreads>0){
        pthread_cond_broadcast(&waitCond);
    }
    if(ret==0){
        pthread_mutex_unlock(&queueMutex);
    }
    LOGV("Tasks Size=%d",pendingTasks);
    for(int N=pendingTasks<poolSize?pendingTasks:poolSize;runningThreads<N;){
        triggerNewThread();
    }
}
void FixedThreadPool::submit(void *args) {
    pthread_mutex_lock(&queueMutex);
    if(isFinishing){
        pthread_mutex_unlock(&queueMutex);
        return;
    }
    ++pendingTasks;
    curElement=new Element(args,curElement);
    pthread_mutex_unlock(&queueMutex);
}
volatile FixedThreadPool::Element* FixedThreadPool::pollNewTask() {
    pthread_mutex_lock(&queueMutex);
    volatile Element* newElement= curElement;
    if(newElement== nullptr){
        if(!isFinishing){
            ++waitingThreads;
            clock_gettime(CLOCK_REALTIME, &realTime);
            realTime.tv_sec+=waitTime/1000;
            realTime.tv_nsec+=(waitTime%1000)*1000000;
            pthread_cond_timedwait(&waitCond,&queueMutex,&realTime);
            ++waitingThreads;
            newElement=curElement;
            if(newElement!= nullptr){
                curElement=newElement->nextElement;
                --pendingTasks;
            }
        }
    } else{
        curElement=newElement->nextElement;
        --pendingTasks;
    }
    pthread_mutex_unlock(&queueMutex);
    return newElement;
}
void* FixedThreadPool::threadFunc(void *args) {

    FixedThreadPool* pool= (FixedThreadPool *) args;
    if(pool->onInit!= nullptr){
        pool->onInit();
    }
    volatile Element* element;
    while ((element = pool->pollNewTask())!= nullptr){
        pool->run(element->args);
        delete element;
    }
    if(pool->onDestroy!= nullptr){
        pool->onDestroy();
    }
    --pool->runningThreads;
    LOGV("Now threads=%d",pool->runningThreads.load());
    if(pool->runningThreads==0){
        pthread_mutex_lock(&pool->queueMutex);
        pthread_cond_signal(&pool->queueCond);
        pthread_mutex_unlock(&pool->queueMutex);
    }
    return nullptr;
}

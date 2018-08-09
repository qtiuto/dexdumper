//
// Created by Karven on 2017/2/25.
//
#include <jni.h>
#include "MyLog.h"
#include "safesocket.h"

#ifndef UTIL_MY_LOG_CPP
#define UTIL_MY_LOG_CPP
#define MAX_LOG_LENGTH 1024
#define LOG_PORT 12786
#define LOG_REMOTE_CONFIG 8

struct MyLog{
    bool enableRemote= false;
    SafeSocket sockClient;
    MyLog():sockClient(LOG_PORT,"127.0.0.1"){}
};
MyLog* myLog;
extern void log_init(){
    if(myLog== nullptr) myLog=new MyLog();
}
extern void log_end(){
    if(myLog!=nullptr){
        delete myLog;
        myLog= nullptr;
    }
}
extern "C"{
JNIEXPORT void JNICALL Java_com_oslorde_extra_MyLog_enableRemoteLogNative(JNIEnv *env, jclass thisClass, jboolean enable){
    if(myLog== nullptr) log_init();
    //myLog->enableRemote=enable;
}
JNIEXPORT void JNICALL Java_com_oslorde_extra_MyLog_logNative(JNIEnv *env, jclass thisClass,jint priority, jstring jTag,jstring jMsg){
    const char* tag=env->GetStringUTFChars(jTag,NULL);
    const char* msg=env->GetStringUTFChars(jMsg,NULL);
    if(priority>LOG_REMOTE_CONFIG||priority<=0){
        priority=ANDROID_LOG_VERBOSE;
    }
    logMsg((android_LogPriority) priority, tag, msg);
    env->ReleaseStringUTFChars(jTag,tag);
    env->ReleaseStringUTFChars(jMsg,msg);
}
}


void log_print(android_LogPriority priority, const char* tag, const char *fmt, ...){
    va_list args;
    va_start(args,fmt);
    log_vprint(priority,tag,fmt,args);
    va_end(args);
}
void log_vprint(android_LogPriority priority, const char* tag, const char *fmt,va_list ap){
    if(!myLog->enableRemote){
        __android_log_vprint(priority,tag,fmt,ap);
        return;
    }
    char log_buf[MAX_LOG_LENGTH];
    vsnprintf(log_buf,MAX_LOG_LENGTH,fmt,ap);
    logMsg(priority,tag,log_buf);
}
void logMsg(android_LogPriority priority, const char* tag, const std::string &msg){
    logMsg(priority,tag,&msg[0]);
}
void logMsg(android_LogPriority priority, const char* tag, const char *msg){
     if(!myLog->enableRemote){
         __android_log_print(priority,tag,"%s",msg);
         return;
     }
    if(!myLog->sockClient.checkConnection()){
        __android_log_print(priority,tag,"%s",msg);
        return;
    }

    struct {
        int32_t priority;
        int32_t tagLen;
        int32_t msgLen;
        int32_t thread_id;
    } logFlags={
            priority,
            (int32_t) strlen(tag),
            (int32_t) strlen(msg),
            gettid()
    };
    myLog->sockClient.send(&logFlags, sizeof(logFlags));
    myLog->sockClient.send( tag, (size_t) logFlags.tagLen);
    myLog->sockClient.send( msg, (size_t) logFlags.msgLen);
    return;

}


#endif //UTIL_MY_LOG_CPP
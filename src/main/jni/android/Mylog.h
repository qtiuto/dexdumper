#ifndef HOOKMANAGER_LOG_H
#define HOOKMANAGER_LOG_H

#include <android/log.h>

#define LOGE(msg, ...) __android_log_print(ANDROID_LOG_ERROR,"Oslorde_DexDump",msg,##__VA_ARGS__)
#define LOGV(msg, ...) __android_log_print(ANDROID_LOG_VERBOSE,"Oslorde_DexDump",msg,##__VA_ARGS__)
#define LOGI(msg, ...)__android_log_print(ANDROID_LOG_INFO,"Oslorde_DexDump",msg,##__VA_ARGS__)
#define LOGW(msg, ...)__android_log_print(ANDROID_LOG_WARN,"Oslorde_DexDump",msg,##__VA_ARGS__)

#endif //HOOKMANAGER_LOG_H
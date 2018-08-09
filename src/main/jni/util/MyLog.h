#include <android/log.h>
#include <string>
#ifndef HOOKMANAGER_LOG_H
#define HOOKMANAGER_LOG_H



#define LOGE(msg, ...) log_print(ANDROID_LOG_ERROR,"Oslorde_DexDump" __FILE__,msg,##__VA_ARGS__)
#define LOGV(msg, ...) log_print(ANDROID_LOG_VERBOSE,"Oslorde_DexDump" ,msg,##__VA_ARGS__)
#define LOGI(msg, ...) log_print(ANDROID_LOG_INFO,"Oslorde_DexDump" ,msg,##__VA_ARGS__)
#define LOGW(msg, ...) log_print(ANDROID_LOG_WARN,"Oslorde_DexDump" __FILE__,msg,##__VA_ARGS__)
#define LOGC(msg) logMsg(ANDROID_LOG_VERBOSE,"Oslorde_DexDump",msg)

extern void log_print(android_LogPriority priority, const char* tag, const char *fmt, ...);
extern void log_vprint(android_LogPriority priority, const char* tag, const char *fmt,va_list ap);
extern void logMsg(android_LogPriority priority, const char* tag, const char *msg);
extern void logMsg(android_LogPriority priority, const char* tag, const std::string &msg);

extern void log_init();
extern void log_end();
#endif //HOOKMANAGER_LOG_H
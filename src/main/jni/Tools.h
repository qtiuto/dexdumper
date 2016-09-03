//
// Created by asus on 2016/8/16.
//

#ifndef HOOKMANAGER_TOOLS_H
#define HOOKMANAGER_TOOLS_H

#include "globals.h"
#include "dex_file.h"
#include "android/log.h"

#define LOGE(msg, ...) __android_log_print(ANDROID_LOG_ERROR,"Oslorde:DexDump",msg,##__VA_ARGS__)
#define LOGV(msg, ...) __android_log_print(ANDROID_LOG_VERBOSE,"Oslorde:DexDump",msg,##__VA_ARGS__)
#define LOGI(msg,...)__android_log_print(ANDROID_LOG_INFO,"Oslorde:DexDump",msg,##__VA_ARGS__)
#define LOGW(msg,...)__android_log_print(ANDROID_LOG_WARN,"Oslorde:DexDump",msg,##__VA_ARGS__)

int parsePositiveDecimalInt(const char *str);
char * my_strrev(char *str);
int readUnsignedLeb128(int &size, const u1 *&ptr);
int readUnsignedLeb128(const u1 *ptr,int& size);
int readSignedLeb128(int& size,const u1 *&ptr);
int readSignedLeb128(const u1 *ptr,int& size);
int unsignedLeb128Size(u4 data);
u1* writeUnsignedLeb128(u1* ptr, u4 data);
void writeUnsignedLeb128ToFile(int fd,u4 data,u4 f_offset);
const char* getStringFromStringId(const art::DexFile::StringId& stringId,const u1* begin);

char* getProtoSig(const art::DexFile::ProtoId& protoId,const art::DexFile::TypeId* typeIds,
                  const art::DexFile::StringId* stringIds,const u1* begin);
char *toJavaClassName(const char *clsChars);
std::string &getProtoString(const art::DexFile::ProtoId &protoId,
                            const art::DexFile::TypeId *typeIds,
                            const art::DexFile::StringId *stringIds, const u1 *begin,
                            std::string &protoType);
void logMethod(const art::DexFile::MethodId& methodId,const art::DexFile* dexFile);
#endif //HOOKMANAGER_TOOLS_H

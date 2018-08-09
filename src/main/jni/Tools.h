//
// Created by asus on 2016/8/16.
//

#ifndef HOOKMANAGER_TOOLS_H
#define HOOKMANAGER_TOOLS_H

#include "support/globals.h"
#include "support/dex_file.h"
#include "util/MyLog.h"
#include "base/macros.h"
#include "utf.h"
#include <unistd.h>
#include <pthread.h>
class JavaString{
    u2* chars;
    size_t count;
public:
    JavaString(const JavaString& other)= delete;
    explicit JavaString(char* mUtf8Chars):JavaString(mUtf8Chars,strlen(mUtf8Chars)){
    }
    explicit JavaString(char* mUtf8Chars,size_t utf8_char_count){
        count=CountModifiedUtf8Chars(mUtf8Chars,utf8_char_count);
        chars=new u2[count];
        ConvertModifiedUtf8ToUtf16(chars,count,mUtf8Chars,utf8_char_count);
    }
    JavaString(JavaString&& other){
        chars=other.chars;
        count=other.count;
        other.chars= nullptr;
    }

    ALWAYS_INLINE u2& operator[](size_t pos){
        return chars[pos];
    }

    ALWAYS_INLINE size_t Count(){ return count;}

    std::string toUtf8(){
        size_t utf8Len=CountUtf8Bytes(chars,count);
        std::string ret;
        ret.resize(utf8Len);
        ConvertUtf16ToModifiedUtf8(&ret[0],utf8Len,chars,count);
        return ret;
    }

    ~JavaString(){
        delete [] chars;
    }
};
std::string JniShortName(const char* className,const char* methodName);
std::string JniLongName(std::string& shortName,char* sig);
inline std::string JniLongName(const char* className,const char* methodName,char* sig){
    std::string shortName(JniShortName(className,methodName));
    return JniLongName(shortName,sig);
}
void skipULeb128(const uint8_t *&ptr);
int parsePositiveDecimalInt(const char *str);
char * my_strrev(char *str);
int readUnsignedLeb128(int &size, const u1 *&ptr);
int readUnsignedLeb128(const u1 *ptr,int& size);
int readSignedLeb128(int& size,const u1 *&ptr);
int readSignedLeb128(const u1 *ptr,int& size);
int unsignedLeb128Size(u4 data);
u1* writeUnsignedLeb128(u1* ptr, u4 data);

int dexUtf8Cmp(const char *s1, const char *s2);

std::string getProtoSig(const u4 index, const art::DexFile *dexFile);
JavaString toJavaClassName(const char *clsChars);

void getProtoString(const art::DexFile::ProtoId &protoId,
                    const art::DexFile* dexFile, std::string &protoType);
void logMethod(const art::DexFile::MethodId& methodId,const art::DexFile* dexFile);
std::string formatMethod(const art::DexFile::MethodId& methodId, const art::DexFile* dexFile);


#endif //HOOKMANAGER_TOOLS_H

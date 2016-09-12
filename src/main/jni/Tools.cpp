//
// Created by asus on 2016/8/16.
//

#include "Tools.h"


char* getProtoSig(const art::DexFile::ProtoId& protoId, const art::DexFile* dexFile){
    using namespace art;
    std::string protoType("(");
    getProtoString(protoId,dexFile, protoType);
    protoType+=")";
    const char* protoString=dexFile->getStringFromTypeIndex(protoId.return_type_idx_);
    protoType+=protoString;
    char* buf=new char[protoType.length()+1];
    strncpy(buf,protoType.c_str(),protoType.length()+1);
    return buf;
}

std::string &getProtoString(const art::DexFile::ProtoId &protoId,const art::DexFile* dexFile,std::string &protoType) {
    if(protoId.parameters_off_ != 0){
        const art::DexFile::TypeList* list= reinterpret_cast<const art::DexFile::TypeList*>(protoId.parameters_off_ + dexFile->begin_);
        int size=list->Size();
        for(u4 i=0;i<size;++i){
            const art::DexFile::TypeItem &item=list->GetTypeItem(i);
            protoType+=dexFile->getStringFromTypeIndex(item.type_idx_);
        };
    }
    return protoType;
}
void logMethod(const art::DexFile::MethodId& methodId, const art::DexFile* dexFile){
    char* sig=getProtoSig(dexFile->proto_ids_[methodId.proto_idx_],dexFile);
    LOGW("Log method class=%s method=%s%s", dexFile->getStringFromTypeIndex(methodId.class_idx_),
         dexFile->getStringByStringIndex(methodId.name_idx_),sig);
    delete [] sig;
}

char *toJavaClassName(const char *clsChars) {
    int len= (int) (strlen(clsChars) - 1);
    char *fixedClssName=new char[len];
    memcpy(fixedClssName,clsChars+1, (size_t) len);
    fixedClssName[len-1]='\0';
    while (*fixedClssName!='\0'){
        if(*fixedClssName=='/')
            *fixedClssName='.';
        ++fixedClssName;
    }
    fixedClssName-=(len-1);
    return fixedClssName;
}

 int readUnsignedLeb128(int &size,const u1 *&ptr) {
    int value=readUnsignedLeb128(ptr, size);
    ptr+=size;
    return value;
}
 int readUnsignedLeb128(const u1 *ptr,int& size) {
    int result = *(ptr++);
    size = 1;
    if (result > 0x7f) {
        int cur = *(ptr++);
        result = (result & 0x7f) | ((cur & 0x7f) << 7);
        ++size;
        if (cur > 0x7f) {
            cur = *(ptr++);
            result |= (cur & 0x7f) << 14;
            ++size;
            if (cur > 0x7f) {
                cur = *(ptr++);
                result |= (cur & 0x7f) << 21;
                ++size;
                if (cur > 0x7f) {
                    /*
                     * Note: We don't check to see if cur is out of
                     * range here, meaning we tolerate garbage in the
                     * high four-order bits.
                     */
                    cur = *(ptr);
                    result |= cur << 28;
                    ++size;
                }
            }
        }
    }
    return result;
}
int readSignedLeb128(int& size,const u1 *&ptr){
    int value=readSignedLeb128(ptr, size);
    ptr+=size;
    return value;
}
 int readSignedLeb128(const u1 *ptr,int& size) {
    int result = *(ptr++);
    size = 1;
    if (result <= 0x7f) {
        result = (result << 25) >> 25;
    } else {
        int cur = *(ptr++);
        result = (result & 0x7f) | ((cur & 0x7f) << 7);
        ++size;
        if (cur <= 0x7f) {
            result = (result << 18) >> 18;
        } else {
            cur = *(ptr++);
            result |= (cur & 0x7f) << 14;
            ++size;
            if (cur <= 0x7f) {
                result = (result << 11) >> 11;
            } else {
                cur = *(ptr++);
                result |= (cur & 0x7f) << 21;
                ++size;
                if (cur <= 0x7f) {
                    result = (result << 4) >> 4;
                } else {
                    /*
                     * Note: We don't check to see if cur is out of
                     * range here, meaning we tolerate garbage in the
                     * high four-order bits.
                     */
                    cur = *(ptr);
                    result |= cur << 28;
                    ++size;
                }
            }
        }
    }
    return result;
}
u1* writeUnsignedLeb128(u1* ptr, u4 data)
{
    while (true) {
        u1 out = (u1) (data & 0x7f);
        if (out != data) {
            *ptr++ = (u1) (out | 0x80);
            data >>= 7;
        } else {
            *ptr++ = out;
            break;
        }
    }
    return ptr;
}
void writeUnsignedLeb128ToFile(int fd,u4 data,u4 offset){
    u1 pStream[5];
    u1* end=writeUnsignedLeb128(pStream,data);
    pwrite(fd,pStream,end-pStream,offset);
}
 int unsignedLeb128Size(u4 data)
{
    int count = 0;

    do {
        data >>= 7;
        count++;
    } while (data != 0);

    return count;
}
char * my_strrev(char *str) {
    char *right = str;
    char *left = str;
    char ch;
    while (*right)   right++;
    right--;
    while (left < right)
    {
        ch = *left;
        *left++ = *right;
        *right-- = ch;
    }
    return(str);
}
int parsePositiveDecimalInt(const  char *str) {
    int value=0;
    while (*str != '\0'){
        if(*str<'\0'||*str>'9'){
            value=-1;
            break;
        }
        value=value*10+(*str-'0');
        ++str;
    }
    return value;
}
void skipULeb128(const uint8_t *&ptr){
    if (*ptr++ > 0x7f){
        if (*ptr++>0x7f){
            if (*ptr++>0x7f){
                if (*ptr++>0x7f){
                    /*
                     * Note: We don't check to see if cur is out of
                     * range here, meaning we tolerate garbage in the
                     * high four-order bits.
                     */
                    ++ptr;
                }
            }
        }
    }
}

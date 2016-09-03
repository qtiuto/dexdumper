//
// Created by asus on 2016/8/16.
//

#include "Tools.h"

std::string &getProtoString(const art::DexFile::ProtoId &protoId,
                            const art::DexFile::TypeId *typeIds,
                            const art::DexFile::StringId *stringIds, const u1 *begin,
                            std::string &protoType);

char* getProtoSig(const art::DexFile::ProtoId& protoId, const art::DexFile::TypeId* typeIds,
                  const art::DexFile::StringId* stringIds, const u1* begin){
    using namespace art;
    std::string protoType("(");
    getProtoString(protoId, typeIds, stringIds, begin, protoType);
    protoType+=")";
    const DexFile::TypeId& typeId=typeIds[protoId.return_type_idx_];
    const char* protoString=getStringFromStringId(stringIds[typeId.descriptor_idx_],begin);
    protoType+=protoString;
    char* buf=new char[protoType.length()+1];
    strncpy(buf,protoType.c_str(),protoType.length()+1);
    return buf;
}

std::string &getProtoString(const art::DexFile::ProtoId &protoId,
const art::DexFile::TypeId *typeIds,
const art::DexFile::StringId *stringIds, const u1 *begin,
std::string &protoType) {
    if(protoId.parameters_off_ != 0){
        const art::DexFile::TypeList* list= reinterpret_cast<const art::DexFile::TypeList*>(protoId.parameters_off_ + begin);
        int size=list->Size();
        for(u4 i=0;i<size;++i){
            const art::DexFile::TypeItem &item=list->GetTypeItem(i);
            const art::DexFile::TypeId &typeId=typeIds[item.type_idx_];
            const char* protoString=getStringFromStringId(stringIds[typeId.descriptor_idx_],begin);
            protoType+=protoString;
        };
    }
    return protoType;
}
void logMethod(const art::DexFile::MethodId& methodId, const art::DexFile* dexFile){
    char* sig=getProtoSig(dexFile->proto_ids_[methodId.proto_idx_],dexFile->type_ids_,dexFile->string_ids_,dexFile->begin_);
    LOGW("Log method class=%s method=%s%s", getStringFromStringId(dexFile->
            string_ids_[dexFile->type_ids_[methodId.class_idx_].descriptor_idx_],dexFile->begin_), getStringFromStringId(
            dexFile->string_ids_[methodId.name_idx_],dexFile->begin_),sig);
    delete [] sig;
}
const char* getStringFromStringId(const art::DexFile::StringId& stringId,const u1* begin){
    int size;const u1* ptr=begin + stringId.string_data_off_;
    readUnsignedLeb128(size,ptr);
    return (const char*)ptr;
}
char *toJavaClassName(const char *clsChars) {
    int len= strlen(clsChars) -1;
    char *fixedClssName=new char[len];
    memcpy(fixedClssName,clsChars+1,len);
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
                    cur = *(ptr++);
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

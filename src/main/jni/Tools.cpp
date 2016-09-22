//
// Created by asus on 2016/8/16.
//

#include "Tools.h"

char *getProtoSig(const u4 index, const art::DexFile *dexFile) {
    using namespace art;
    std::string protoType("(");
    if (index >= dexFile->header_->proto_ids_size_) {
        throw std::out_of_range(formMessage("std::out_of_range:ProtoIndex", index));
    }
    auto &protoId = dexFile->proto_ids_[index];
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
    char *sig = getProtoSig(methodId.proto_idx_, dexFile);
    LOGW("Log method class=%s method=%s%s", dexFile->getStringFromTypeIndex(methodId.class_idx_),
         dexFile->getStringByStringIndex(methodId.name_idx_),sig);
    delete [] sig;
}

char *toJavaClassName(const char *clsChars) {
    bool notArray = clsChars[0] != '[';
    int len = (int) (strlen(clsChars));
    if (notArray) --len;
    else ++len;
    char *fixedClssName=new char[len];
    memcpy(fixedClssName, clsChars + notArray, (size_t) len);
    fixedClssName[len-1]='\0';
    while (*fixedClssName!='\0'){
        if(*fixedClssName=='/')
            *fixedClssName='.';
        ++fixedClssName;
    }
    fixedClssName-=(len-1);
    return fixedClssName;
}

u2 dexGetUtf16FromUtf8(const char **pUtf8Ptr) {
    unsigned int one, two, three;

    one = *(*pUtf8Ptr)++;
    if ((one & 0x80) != 0) {
        /* two- or three-byte encoding */
        two = *(*pUtf8Ptr)++;
        if ((one & 0x20) != 0) {
            /* three-byte encoding */
            three = *(*pUtf8Ptr)++;
            return (u2) (((one & 0x0f) << 12) |
                         ((two & 0x3f) << 6) |
                         (three & 0x3f));
        } else {
            /* two-byte encoding */
            return (u2) (((one & 0x1f) << 6) |
                         (two & 0x3f));
        }
    } else {
        /* one-byte encoding */
        return (u2) one;
    }
}

int dexUtf8Cmp(const char *s1, const char *s2) {
    for (; ;) {
        if (*s1 == '\0') {
            if (*s2 == '\0') {
                return 0;
            }
            return -1;
        } else if (*s2 == '\0') {
            return 1;
        }

        int utf1 = dexGetUtf16FromUtf8(&s1);
        int utf2 = dexGetUtf16FromUtf8(&s2);
        int diff = utf1 - utf2;

        if (diff != 0) {
            return diff;
        }
    }
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

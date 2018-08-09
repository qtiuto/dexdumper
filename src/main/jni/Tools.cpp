//
// Created by asus on 2016/8/16.
//

#include <cstring>
#include <algorithm>
#include "Tools.h"
#include "utf-inl.h"
std::string getProtoSig(const u4 index, const art::DexFile *dexFile) {
    using namespace art;
    std::string protoType("(");
    if (index >= dexFile->header_->proto_ids_size_) {
        throw std::out_of_range(formMessage("std::out_of_range:ProtoIndex", index));
    }
    auto &protoId = dexFile->proto_ids_[index];
    getProtoString(protoId,dexFile, protoType);
    protoType+=")";
    const char* retType= dexFile->stringFromType(protoId.return_type_idx_);
    protoType+=retType;
    return protoType;
}

void getProtoString(const art::DexFile::ProtoId &protoId, const art::DexFile *dexFile,
                    std::string &protoType) {
    if(protoId.parameters_off_ != 0){
        const art::DexFile::TypeList* list= reinterpret_cast<const art::DexFile::TypeList*>(protoId.parameters_off_ + dexFile->begin_);
        int size=list->Size();
        for(u4 i=0;i<size;++i){
            const art::DexFile::TypeItem &item=list->GetTypeItem(i);
            protoType+= dexFile->stringFromType(item.type_idx_);
        };
    }
}
std::string formatMethod(const art::DexFile::MethodId& methodId, const art::DexFile* dexFile){
    return formMessage(dexFile->stringFromType(methodId.class_idx_),"->", dexFile->stringByIndex(methodId.name_idx_),
                getProtoSig(methodId.proto_idx_, dexFile));
}

void logMethod(const art::DexFile::MethodId& methodId, const art::DexFile* dexFile){
    LOGW("Log method :%s->%s%s", dexFile->stringFromType(methodId.class_idx_),
         dexFile->stringByIndex(methodId.name_idx_),
         getProtoSig(methodId.proto_idx_, dexFile).c_str());
}
class UTFDataFormatException :public std::runtime_error{
public:
    UTFDataFormatException(const char * msg) : runtime_error(msg){ }
};


JavaString toJavaClassName(const char *clsChars) {
    bool notArray = clsChars[0] != '[';
    size_t len = strlen(clsChars);
    if (notArray) --len;
    else ++len;
    char fixedClssName[len];
    memcpy(fixedClssName, clsChars + notArray, (size_t) len);
    fixedClssName[len-1]='\0';
    for(char& ch:fixedClssName){
        if(ch=='/') ch='.';
    }
    JavaString ret(fixedClssName,len-1);
    return ret;
}
std::string MangleForJni(const std::string& s) {
    std::string result;
    size_t char_count = CountModifiedUtf8Chars(s.c_str());
    const char* cp = &s[0];
    for (size_t i = 0; i < char_count; ++i) {
        uint32_t ch = GetUtf16FromUtf8(&cp);
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            result.push_back(ch);
        } else if (ch == '.' || ch == '/') {
            result += "_";
        } else if (ch == '_') {
            result += "_1";
        } else if (ch == ';') {
            result += "_2";
        } else if (ch == '[') {
            result += "_3";
        } else {
            const uint16_t leading = GetLeadingUtf16Char(ch);
            const uint32_t trailing = GetTrailingUtf16Char(ch);

            char out[7]={0};
            sprintf(out,"_0%04x",leading);
            result.append(out,6);
            if (trailing != 0) {
                sprintf(out,"_0%04x",trailing);
                result.append(out,6);
            }
        }
    }
    return result;
}

std::string DotToDescriptor(const char* class_name) {
    std::string descriptor(class_name);
    std::replace(descriptor.begin(), descriptor.end(), '.', '/');
    if (descriptor.length() > 0 && descriptor[0] != '[') {
        descriptor = "L" + descriptor + ";";
    }
    return descriptor;
}

std::string DescriptorToDot(const char* descriptor) {
    size_t length = strlen(descriptor);
    if (length > 1) {
        if (descriptor[0] == 'L' && descriptor[length - 1] == ';') {
            // Descriptors have the leading 'L' and trailing ';' stripped.
            std::string result(descriptor + 1, length - 2);
            std::replace(result.begin(), result.end(), '/', '.');
            return result;
        } else {
            // For arrays the 'L' and ';' remain intact.
            std::string result(descriptor);
            std::replace(result.begin(), result.end(), '/', '.');
            return result;
        }
    }
    // Do nothing for non-class/array descriptors.
    return descriptor;
}

std::string DescriptorToName(const char* descriptor) {
    size_t length = strlen(descriptor);
    if (descriptor[0] == 'L' && descriptor[length - 1] == ';') {
        std::string result(descriptor + 1, length - 2);
        return result;
    }
    return descriptor;
}

std::string JniShortName(const char* className,const char* methodName) {
    std::string class_name(className);
    // Remove the leading 'L' and trailing ';'...

    class_name.erase(0, 1);
    class_name.erase(class_name.size() - 1, 1);

    std::string method_name(methodName);

    std::string short_name;
    short_name += "Java_";
    short_name += MangleForJni(class_name);
    short_name += "_";
    short_name += MangleForJni(method_name);
    return short_name;
}

std::string JniLongName(std::string& shortName,char* sig){
    std::string long_name;
    long_name +=shortName;
    long_name += "__";
    std::string signature(sig);
    signature.erase(0, 1);
    signature.erase(signature.begin() + signature.find(')'), signature.end());

    long_name += MangleForJni(signature);

    return long_name;
}

ALWAYS_INLINE int dexUtf8Cmp(const char *s1, const char *s2) {
    return CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(s1,s2);
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

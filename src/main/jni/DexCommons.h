//
// Created by asus on 2016/8/16.
//
#include "Tools.h"
#include "base/macros.h"
#include "util/FixedThreadPool.h"
#include "DexSeeker.h"
#include <fcntl.h>
#include <cstdlib>
#include <sys/syscall.h>
#include <assert.h>

#ifndef HOOKMANAGER_COMMONS_H
#define HOOKMANAGER_COMMONS_H

#define PACKED_SWITCH 1
#define SPARSE_SWITCH 2
#define FILL_ARRAY_DATA 3
enum {
    UNDEFINED = 0xffffffff,
};
enum DumpMode{
    MODE_LOOSE=0x0,
    MODE_DIRECT=0x1,
    MODE_FIX_CODE=0x2,
    MODE_THROW_TO_JAVA = 0x4
};
enum SDK_OPT{
    DALVIK,
    ART_KITKAT,
    ART_LOLLIPOP,
    ART_LOLLIPOP_MR1,
    ART_MARSHMALLOW,
    ART_NOUGAT
};
struct __attribute__ ((packed)) DataSection {
    u1 prePaddingSize=0;
    u1 postPaddingSize=0;
    u2 type=0;
    u4 size=0;
    u4 fileOffset=0;
    union PACKED(1){
        u4 parOffset=0;
        u4 parIdx;
    };
    const void* src= nullptr;
    union PACKED(1){
        DataSection* parRef= nullptr;
        unsigned long parStart;
    };
    //ref: reference to the address of offset , nullptr if no need to re-order
};
struct PACKED(1) ULebRef{
    u1 origSize;
    u1 nowSize;
    u4 offset;
};
struct __attribute__ ((packed)) ClassDataSection:public DataSection{
    u2 codeRefSize=0;
    ULebRef codeRefs[];
};
struct __attribute__ ((packed)) CodeItemSect:public DataSection{
    u4* fileOffsetRef= nullptr;
    bool isValidDebugOff = true;
    void updateFileOffset(u4 fileOff){
        if(fileOffsetRef!= nullptr)
            *fileOffsetRef=fileOff+offsetof(art::DexFile::CodeItem,insns_);
        fileOffset=fileOff;
    }
};
struct DexGlobal{
private:
    jclass toolsClass;
    jmethodID firstInitId;
    jmethodID getMethodId;
    jmethodID getFieldId;
public:
    u1 poolSize=1;
    u1 dumpMode;
    u2 sdkOpt;
    FixedThreadPool* pool= nullptr;
    DexSeeker* dexSeeker= nullptr;
    char* dexFileName;
    const art::DexFile* dexFile;


    void initPoolIfNeeded(void* (* run_)(void* args),
                          void (*onInit_)()= nullptr, void (*onDestroy_)()= nullptr){
        if(pool== nullptr){
            pool=new FixedThreadPool(poolSize,200,run_,onInit_,onDestroy_);
            setupDexSeeker();
        }

    }

     void setupDexSeeker() {
        if(dexSeeker!= nullptr){
            delete dexSeeker;
        }
        dexSeeker =new DexSeeker;
    }

    //only set once to cross native thread boundary.
    void setToolsClass(JNIEnv* env){
        if(toolsClass== nullptr){
            jclass tools=env->FindClass("com/oslorde/extra/ClassTools");
            getFieldId = env->GetStaticMethodID(tools, "getFieldFromOffset",
                                                "(Ljava/lang/String;I)[B");
            getMethodId = env->GetStaticMethodID(tools, "getMethodFromIndex",
                                                 "(Ljava/lang/String;I)[B");
            firstInitId=env->GetStaticMethodID(tools, "findFirstInit", "(Ljava/lang/String;)Ljava/lang/reflect/Member;");
            toolsClass= (jclass) env->NewGlobalRef(tools);
            env->DeleteLocalRef(tools);
        }
    }

    bool isThrowToJava() {
        return (dumpMode & MODE_THROW_TO_JAVA) != 0;
    }

    const jclass getToolsClass() {
        return toolsClass;
    }

    const jmethodID getGetMethodID() {
        return getMethodId;
    }

    const jmethodID getGetFieldID() {
        return getFieldId;
    }

    const jmethodID getFirstInitID() {
        return firstInitId;
    }

    void release(JNIEnv *env) {
        env->DeleteGlobalRef(toolsClass);
        toolsClass= nullptr;
        delete pool;
        delete dexSeeker;
        pool= nullptr;
        dexSeeker= nullptr;
    }
    ~DexGlobal(){
        if(toolsClass!= nullptr){
            throw "Tools Class must be freed";
        }

    }
};
extern DexGlobal dexGlobal;
extern thread_local bool isLog;
extern const char fill[];
inline bool isFixCode(){
    return (dexGlobal.dumpMode&MODE_FIX_CODE)!=0;
}
#define isDalvik() (dexGlobal.sdkOpt==DALVIK)

#define isKitkatArt() (dexGlobal.sdkOpt==ART_KITKAT)

#define isArtL() (dexGlobal.sdkOpt==ART_LOLLIPOP)

#define isArtLMr1()  (dexGlobal.sdkOpt == ART_LOLLIPOP_MR1)

inline bool isArtM() {
    return dexGlobal.sdkOpt == ART_MARSHMALLOW;
};
#define isArtNougat() (dexGlobal.sdkOpt==ART_NOUGAT)
inline bool equals(const char* s1, const char* s2){
    return strcmp(s1,s2)==0;
}

#endif
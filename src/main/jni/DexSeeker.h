//
// Created by Karven on 2016/10/15.
//

#ifndef DEXDUMP_DEXSEEKER_H
#define DEXDUMP_DEXSEEKER_H

#include <unordered_map>
#include <vector>
#include "util/SpinLock.h"
#include "support/globals.h"
#include "support/dex_file.h"
#include "support/art-member.h"


class DexSeeker{
    SpinLock lock;
    typedef std::unordered_map<u4,u4*> ClassMap;
    typedef std::unordered_map<u4,void*> FieldCache;
    typedef std::unordered_map<u4,FieldCache*> FieldMap;
    ClassMap classMap;
    FieldMap fieldMap;

    void addArtClassList(std::vector<const char *> &classes, ArtClass *artClass);
public:
    void* getClass(JNIEnv *env, u4 classIdx);
    u4 getMethodByIndex(JNIEnv* env,u4 classIdx,u4 vIdx);
    u4 getFieldIdFromOffset(JNIEnv* env,u4 classIdx,u4 offset);
    ~DexSeeker(){
        for(auto& pair:fieldMap){
            delete pair.second;
        }
    }
};



#endif //DEXDUMP_DEXSEEKER_H

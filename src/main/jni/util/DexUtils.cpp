//
// Created by Karven on 2016/10/14.
//

#include "DexUtils.h"
#include "../DexCommons.h"


art::DexFile *getRealDexFile(ArtClass* artClass) {
    assert(!isDalvik());
    u4 dexCachePtr = artClass->getDexCache(isArtNougat());
    if (isArtNougat()) {
        u8 dexFile =  reinterpret_cast<NougatDexCache *>(dexCachePtr)->dex_file_;
        return (art::DexFile *)dexFile;
    }
#ifndef __LP64__
        else if (isKitkatArt()) {
        return (art::DexFile *) reinterpret_cast<KitkatDexCache *>(dexCachePtr)->dex_file_;
    }
#endif
    else return (art::DexFile *) reinterpret_cast<DexCache *>(dexCachePtr)->dex_file_;
}

u4 binarySearchMethod(const char *className, const char *methodName, const char *retType,
                      const char *parSig) {
    const art::DexFile *dexFile = dexGlobal.dexFile;
    u4 low = 0, high = dexFile->header_->method_ids_size_ - 1, mid;
    int value;
    while (low <= high) {
        mid = (low + high) >> 1;
        auto methodId = dexFile->method_ids_[mid];
        auto name = dexFile->stringFromType(methodId.class_idx_);
        value = dexUtf8Cmp(name, className);
        if (value == 0) {
            name = dexFile->stringByIndex(methodId.name_idx_);
            value = dexUtf8Cmp(name, methodName);
            if (value == 0) {
                const art::DexFile::ProtoId &protoId = dexFile->proto_ids_[methodId.proto_idx_];
                name = dexFile->stringFromType(protoId.return_type_idx_);
                value = dexUtf8Cmp(name, retType);
                if (value == 0) {
                    std::string proto("");
                    getProtoString(protoId, dexFile, proto);
                    value = dexUtf8Cmp(&proto[0], parSig);
                    if (value == 0) return mid;
                };
            }
        }
        if (value > 0) {
            high = mid - 1;
        } else low = mid + 1;
    }
    return UNDEFINED;
}

u4 binarySearchField(const char *className, const char *fieldName, const char *typeName) {
    const art::DexFile *dexFile = dexGlobal.dexFile;
    u4 low = 0, high = dexFile->header_->field_ids_size_ - 1, mid;
    int value;
    while (low <= high) {
        mid = (low + high) >> 1;
        auto fieldId = dexFile->field_ids_[mid];
        auto name = dexFile->stringFromType(fieldId.class_idx_);
        value = dexUtf8Cmp(name, className);
        if (value == 0) {
            name = dexFile->stringByIndex(fieldId.name_idx_);
            value = dexUtf8Cmp(name, fieldName);
            if (value == 0) {
                name = dexFile->stringFromType(fieldId.type_idx_);
                value = dexUtf8Cmp(name, typeName);
                if (value == 0) return mid;
            }
        }
        if (value > 0) {
            high = mid - 1;
        } else low = mid + 1;
    }
    return UNDEFINED;
}
u4 binarySearchType(const char *typeName, const art::DexFile *dexFile) {
    u4 low = 0, high = dexFile->header_->type_ids_size_ - 1, mid;
    int value;
    while (low <= high) {
        mid = (low + high) >> 1;
        auto name = dexFile->stringFromType(mid);
        value = dexUtf8Cmp(name, typeName);
        if (value == 0) {
            return mid;
        } else if (value > 0) {
            high = mid - 1;
        } else low = mid + 1;
    }
    return UNDEFINED;
}

u4 searchClassPos(const char *className) {
    const art::DexFile *dexFile = dexGlobal.dexFile;
    for (u4 i = 0, N = dexFile->header_->class_defs_size_; i < N; ++i) {
        auto name = dexFile->stringFromType(dexFile->class_defs_[i].class_idx_);
        if (equals(className, name)) return i;
    }
    return UNDEFINED;
}
dalvik::DexFile* getDalvikDexFile(dalvik::ClassObject* classObject){
    return classObject->pDvmDex->pDexFile;
}






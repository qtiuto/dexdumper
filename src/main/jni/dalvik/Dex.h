//
// Created by asus on 2016/8/7.
//

#ifndef HOOKMANAGER_DEX_H
#define HOOKMANAGER_DEX_H

#include "DexFile.h"
namespace dalvik{
    struct ClassObject;
    struct StringObject;
    struct Method;
    struct Field;
    struct DvmDex {
        /* pointer to the DexFile we're associated with */
        DexFile*            pDexFile;

        /* clone of pDexFile->pHeader (it's used frequently enough) */
        const DexHeader*    pHeader;

        /* interned strings; parallel to "stringIds" */
        struct StringObject** pResStrings;

        /* resolved classes; parallel to "typeIds" */
        struct ClassObject** pResClasses;

        /* resolved methods; parallel to "methodIds" */
        struct Method**     pResMethods;

        /* resolved instance fields; parallel to "fieldIds" */
        /* (this holds both InstField and StaticField) */
        struct Field**      pResFields;
    };
    struct DexProto {
        const DexFile* dexFile;     /* file the idx refers to */
        u4 protoIdx;                /* index into proto_ids table of dexFile */
    };
    typedef void* ZipEntry;

/*
 * One entry in the hash table.
 */
    struct ZipHashEntry {
        const char*     name;
        unsigned short  nameLen;
    };
    struct MemMapping {
        void*   addr;           /* start of data */
        size_t  length;         /* length of data */

        void*   baseAddr;       /* page-aligned base address */
        size_t  baseLength;     /* length of mapping */
    };
    struct ZipArchive {
        /* open Zip archive */
        int         mFd;

        /* mapped central directory area */
        off_t       mDirectoryOffset;
        MemMapping  mDirectoryMap;

        /* number of entries in the Zip archive */
        int         mNumEntries;

        /*
         * We know how many entries are in the Zip archive, so we can have a
         * fixed-size hash table.  We probe on collisions.
         */
        int         mHashTableSize;
        ZipHashEntry* mHashTable;
    };
    struct JarFile {
        ZipArchive  archive;
        //MemMapping  map;
        char*       cacheFileName;
        DvmDex*     pDvmDex;
    };
    struct RawDexFile {
        char*       cacheFileName;
        DvmDex*     pDvmDex;
    };
    struct DexOrJar {
        char*       fileName;
        bool        isDex;
        bool        okayToFree;
        RawDexFile* pRawDexFile;
        JarFile*    pJarFile;
        u1*         pDexMemory; // malloc()ed memory, if any
    };
}

#endif //HOOKMANAGER_DEX_H

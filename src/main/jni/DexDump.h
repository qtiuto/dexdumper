#include <vector>
#include <sys/types.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include "jni.h"
#include "dex_file.h"
#include "art-member.h"
#include "DexCommons.h"
#include "DexCacheFile.h"

#ifndef _Included_com_oslorde_extra_DexDumper
#define _Included_com_oslorde_extra_DexDumper

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_dumpDexV21(JNIEnv *env,jclass thisClass,jlong dex_vector_address,jstring outDir,jboolean isMr1);
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_dumpDexV23(JNIEnv *env,jclass thisClass,jlongArray dex_files_address,jstring outDir,jboolean isNougat);
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_dumpDexV19ForArt(JNIEnv *env,jclass thisClass,jlong cookie,jstring outDir);
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_dumpDexV16(JNIEnv *env, jclass thisClass,
                                                                    jlong cookie, jstring outDir);
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_setMode(JNIEnv *env,jclass thisClass,jint mode);
#ifdef __cplusplus
}
 enum DexDataType{
     typeAnnotationSetRefList,
     typeAnnotationSetItem,
     typeCodeItem,
     typeAnnotationDirectoryItem,
     typeInterface,//type list
     typeParameter,//type list
     typeStringDataItem,
     typeDebugInfoItem,
     typeAnnotationItem,
     typeStaticValueItem,
     typeClassDataItem,
     typeMapList
};
struct __attribute__ ((packed)) Encoded_Value {
    uint8_t valueArgsAndType;
    uint8_t value[0];
};

struct HeadSection{
    u4 num;
    u4 size;
    u1* ptr;
};
struct LinkData{
    u1* data;
    u4 size;
};
struct ItemState{
    u4 size=0;
    u4 offset=0;
    u2 type;
    bool isFirst= true;
};
enum ValueType {
    kByte = 0x00,
    kShort = 0x02,
    kChar = 0x03,
    kInt = 0x04,
    kLong = 0x06,
    kFloat = 0x10,
    kDouble = 0x11,
    kString = 0x17,
    kType = 0x18,
    kField = 0x19,
    kMethod = 0x1a,
    kEnum = 0x1b,
    kArray = 0x1c,
    kAnnotation = 0x1d,
    kNull = 0x1e,
    kBoolean = 0x1f
};


static short getMapItemTypeFromDataType(u2 type){
    using namespace art;
    switch (type){
        case typeAnnotationItem:
            return DexFile::kDexTypeAnnotationItem;
        case typeCodeItem:
            return DexFile::kDexTypeCodeItem;
        case typeAnnotationDirectoryItem:
            return DexFile::kDexTypeAnnotationsDirectoryItem;
        case typeInterface:
            return DexFile::kDexTypeTypeList;
        case typeParameter:
            return DexFile::kDexTypeTypeList;
        case typeStringDataItem:
            return DexFile::kDexTypeStringDataItem;
        case typeDebugInfoItem:
            return DexFile::kDexTypeDebugInfoItem;
        case typeAnnotationSetRefList:
            return DexFile::kDexTypeAnnotationSetRefList;
        case typeAnnotationSetItem:
            return DexFile::kDexTypeAnnotationSetItem;
        case typeStaticValueItem:
            return DexFile::kDexTypeEncodedArrayItem;
        case typeClassDataItem:
            return DexFile::kDexTypeClassDataItem;
        case typeMapList:
            return DexFile::kDexTypeMapList;
        default:
            return -1;
    }
}

const char* getDexDataTypeName(u2 type){
    const char * dexDataType[]={
            "typeAnnotationSetRefList",
            "typeAnnotationSetItem",
            "typeCodeItem",
            "typeAnnotationDirectoryItem",
            "typeInterface",//type list
            "typeParameter",//type list
            "typeStringDataItem",
            "typeDebugInfoItem",
            "typeAnnotationItem",
            "typeStaticValueItem",
            "typeClassDataItem",
            "typeMapList"
    };
    return dexDataType[type];
}
const char* getDexDataTypeName(u2 type);

static void setDexName(int count, char * dexName);
static void dumpDex(JNIEnv* env,std::vector<const art::DexFile*>& dex_files,const char* outDir);

static int getEncodedAnnotationItemSize(const u1* encodedItem);
static int getEncodedArraySize(const u1* encodedArray);
static int getEncodedValueSize(const Encoded_Value *encodedValue);
static void putTypeList(std::vector<::DataSection * >& dataSection, const long offset,u4 paraStart,  u4 parOffset,
                        u2 type,  u1* begin);
static int skimDebugCode(const u1** pStream);

static bool putCodeItem( std::vector<::DataSection *>& dataSection,
                         art::DexFile::CodeItem* codeItem,CodeItemSect* section, u1* begin);

static short getMapItemTypeFromDataType(u2 type);

static void fixDataSection(std::vector<::DataSection *> &dataSection, art::DexFile::Header *header);

static void writeHeadSection(const HeadSection head[7],  int fd);

static void writeDataSection(DexCacheFile &dexCacheFile, DataSection *section);

static void updateRef(DexCacheFile &dexCacheFile, DataSection *section);
bool compareOffset( DataSection* const &first, DataSection*const &sec);
bool compareDexType( DataSection*const &first, DataSection*const &sec);
static void insertEmptySections(std::vector<::DataSection *> &dataSection);
static void fillMapItem(art::DexFile::MapItem* mapItem,ItemState state[],int& index,u4 &mapCount);
static void fixMapListHeaderPart(const art::DexFile::Header *header, const HeadSection *const heads,
                                 art::DexFile::MapList *mapList);
static void changeItemState(ItemState itemStates[],int index,DataSection *section,u4& size,u4 data_off,u2 type, bool requiredPadding);


static void fixMethodCodeIfNeeded(JNIEnv *env, const art::DexFile *dexFile, int methodSize,
                                  const jclass &thizClass,
                                  std::vector<::DataSection *> &dataSection, const u1 *&ptr,
                                  ClassDataSection *classData,
                                  std::vector<const art::DexFile *> &dex_files);

static void putAnnoSetItem( std::vector<::DataSection *>& dataSection,DataSection* par,
                            u4 parOffset,const art::DexFile::AnnotationSetItem* setItem, u1* begin) ;

static bool judgeVersion(const unsigned char* str);

bool fixOpCodeOrNot(u2 *insns, u4 insns_szie);
#endif
#endif

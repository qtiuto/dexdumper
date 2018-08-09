//
// Created by Karven on 2016/10/14.
//

#ifndef DEXDUMP_DEXUTILS_H_H
#define DEXDUMP_DEXUTILS_H_H

#include "../support/globals.h"
#include "../support/dex_file.h"
#include "../dalvik/DexFile.h"
#include "../dalvik/Object.h"
#include "../support/art-member.h"

#define GET_ART_METHOD_MEMBER_VALUE(var_out,mem_name, method_id)\
 if(isKitkatArt()){\
ArtMethodKitkat* artMethod= reinterpret_cast<ArtMethodKitkat*>(reinterpret_cast<u1*>(method_id));\
var_out= artMethod->mem_name;\
} else if(isArtL()){\
ArtMethodLollipop* artMethod= reinterpret_cast<ArtMethodLollipop*>(reinterpret_cast<u1*>(method_id));\
var_out=  artMethod->mem_name;\
} else if(isArtLMr1()){\
ArtMethodLollipopMr1* artMethod= reinterpret_cast<ArtMethodLollipopMr1*>(reinterpret_cast<u1*>(method_id));\
var_out=  artMethod->mem_name;\
} else if(isArtNougat()){\
ArtMethodNougat* artMethod=reinterpret_cast<ArtMethodNougat*>(reinterpret_cast<u1*>(method_id));\
var_out=  artMethod->mem_name;\
}\
else{\
ArtMethodMarshmallow* artMethod= reinterpret_cast<ArtMethodMarshmallow*>(reinterpret_cast<u1*>(method_id));\
var_out=  artMethod->mem_name;\
}
#define GET_ART_METHOD_MEMBER_PTR(var_out,mem_name, method_id)\
 if(isKitkatArt()){\
ArtMethodKitkat* artMethod= reinterpret_cast<ArtMethodKitkat*>(reinterpret_cast<u1*>(method_id));\
var_out= &artMethod->mem_name;\
} else if(isArtL()){\
ArtMethodLollipop* artMethod= reinterpret_cast<ArtMethodLollipop*>(reinterpret_cast<u1*>(method_id));\
var_out= &artMethod->mem_name;\
} else if(isArtLMr1()){\
ArtMethodLollipopMr1* artMethod= reinterpret_cast<ArtMethodLollipopMr1*>(reinterpret_cast<u1*>(method_id));\
var_out= &artMethod->mem_name;\
} else if(isArtNougat()){\
ArtMethodNougat* artMethod=reinterpret_cast<ArtMethodNougat*>(reinterpret_cast<u1*>(method_id));\
var_out=&artMethod->mem_name;\
}\
else{\
ArtMethodMarshmallow* artMethod= reinterpret_cast<ArtMethodMarshmallow*>(reinterpret_cast<u1*>(method_id));\
var_out= &artMethod->mem_name;\
}

u4 binarySearchMethod(const char *className, const char *methodName, const char *retType,
                      const char *parSig);
u4 binarySearchField(const char *className, const char *fieldName, const char *typeName);
u4 searchClassPos(const char *className);
u4 binarySearchType(const char *typeName, const art::DexFile *dexFile);
art::DexFile *getRealDexFile(ArtClass* artClass);
dalvik::DexFile* getDalvikDexFile(dalvik::ClassObject* classObject);
#endif //DEXDUMP_DEXUTILS_H_H

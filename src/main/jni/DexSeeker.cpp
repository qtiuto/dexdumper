//
// Created by Karven on 2016/10/15.
//

#include "util/DexUtils.h"
#include "DexSeeker.h"
#include "DexCommons.h"

#define GET_ART_CLASS_MEMBER(artClass,out,mem_name)\
    switch (dexGlobal.sdkOpt){\
        case ART_KITKAT:\
            out= static_cast<KClass*>(artClass)->mem_name;\
            break;\
        case ART_LOLLIPOP:\
            out= static_cast<LClass*>(artClass)->mem_name;\
            break;\
        case ART_LOLLIPOP_MR1:\
            out= static_cast<LMr1Class*>(artClass)->mem_name;\
            break;\
        case ART_MARSHMALLOW:\
            out= static_cast<MarClass*>(artClass)->mem_name;\
            break;\
        case ART_NOUGAT:\
            out= reinterpret_cast<NougatClass*>(artClass)->mem_name;\
            break;\
        default:{\
            exit(-2);\
        }\
    }\

template <typename T>
void scanClassList(std::function<T(T)> GetFunc,std::function<bool (T)>& OnClass,T clazz){
    do{
        if(OnClass(clazz)){
            break;
        }
        clazz=GetFunc(clazz);
    }while (clazz!= nullptr);
}

inline void scanArtClassList(std::function<bool(ArtClass*)> onClass,ArtClass* clazz){
    scanClassList<ArtClass*>([](ArtClass* aClass)-> ArtClass*{
        u4 superClass;
        GET_ART_CLASS_MEMBER(aClass,superClass,super_class_)
        return reinterpret_cast<ArtClass*>(superClass);
    }, onClass,clazz);
}
inline void scanDalvikClassList(std::function<bool(dalvik::ClassObject*)> onClass,dalvik::ClassObject* clazz){
    scanClassList<dalvik::ClassObject*>([](dalvik::ClassObject* Object)-> dalvik::ClassObject*{
        return Object->super;
    },onClass,clazz);
}
u4 DexSeeker::getFieldIdFromOffset(JNIEnv* env,u4 classIdx, u4 offset) {
    lock.lock();
    void *clazz = getClass(env, classIdx);
    if (clazz == nullptr) {
        LOGV("can't find class f");
        lock.unlock();
        return UNDEFINED;
    }
    FieldMap::iterator fiterator=fieldMap.find(classIdx);
    FieldCache* map;
    if(fiterator == fieldMap.end()){
        map=new FieldCache();
        fieldMap.emplace(classIdx,map);
        if(isDalvik()){
            scanDalvikClassList([&map](dalvik::ClassObject* classObject)->bool{
                dalvik::InstField* fields=classObject->ifields;
                for(int i=classObject->ifieldCount-1;i>=0;--i){
                    dalvik::InstField* instField=fields+i;
                    map->emplace(instField->byteOffset,instField);
                }
                return false;
            }, (dalvik::ClassObject *) clazz);
        } else{
            scanArtClassList([&map](ArtClass* artClass)-> bool{
                u8 ifields;
                GET_ART_CLASS_MEMBER(artClass,ifields,ifields_)
                if(ifields==0) return false;
                if(dexGlobal.sdkOpt<ART_MARSHMALLOW){
                    ObjectArray* objectArray= (ObjectArray *) ifields;
                    for(int i=0;i<objectArray->size;++i){
                        ArtFieldLollipop** artFieldHandle= reinterpret_cast<ArtFieldLollipop **>(&objectArray->elements[i]) ;
                        map->emplace((*artFieldHandle)->offset_,artFieldHandle);
                    }
                }
                else if(dexGlobal.sdkOpt==ART_MARSHMALLOW){
                    ArtField* array= reinterpret_cast<ArtField*>(ifields) ;
                    MarClass* marClass=(MarClass*) artClass;
                    for (int i=0;i<marClass->num_instance_fields_;++i){
                        map->emplace(array[i].offset_,&array[i]);
                    }
                }
                else{
                    art::Array<ArtField>* array= (art::Array<ArtField> *) ifields;
                    for(size_t i=array->size()-1;i!=-1;--i){
                        ArtField* artField= &array->At((u4) i);
                        map->emplace(artField->offset_,artField);
                    }
                }
                return false;
            },( ArtClass*) clazz);
        }
    } else map=fiterator->second;
    lock.unlock();
    FieldCache::iterator iterator= map->find(offset);
    if(iterator==map->end()){
        return UNDEFINED;
    }
    std::vector<const char*> classes;
    const char* name;
    const char* type;
    if(isDalvik()){
        scanDalvikClassList([&classes](dalvik::ClassObject* object)->bool{
            classes.push_back(object->descriptor);
            return false;
        }, (dalvik::ClassObject *) clazz);
        dalvik::Field* field= (dalvik::Field *) iterator->second;
        name=field->name;
        type=field->signature;
    } else {
        art::DexFile* dexFile;
        const art::DexFile::FieldId* fieldId;
        if(dexGlobal.sdkOpt<ART_MARSHMALLOW){
            ArtFieldLollipop* artField= *reinterpret_cast<ArtFieldLollipop**>(iterator->second) ;
            ArtClass *artClass = (ArtClass *) static_cast<uintptr_t>(artField->declaring_class_);
            dexFile = getRealDexFile(artClass);
            fieldId = &dexFile->field_ids_[artField->field_dex_idx_];
        } else{
            ArtField* artField= (ArtField *) iterator->second;
            ArtClass *artClass = (ArtClass *) static_cast<uintptr_t>(artField->declaring_class_);
            dexFile = getRealDexFile(artClass);
            fieldId = &dexFile->field_ids_[artField->field_dex_idx_];
        }
        name=dexFile->stringByIndex(fieldId->name_idx_);
        type=dexFile->stringFromType(fieldId->type_idx_);
        addArtClassList(classes, (ArtClass *) clazz);
    }
    for(auto className:classes){
        u4 ret=binarySearchField(className,name,type);
        if(ret!=UNDEFINED) return ret;
    }
    return UNDEFINED;

}

inline void DexSeeker::addArtClassList(std::vector<const char *> &classes, ArtClass *artClass)  {
    scanArtClassList([&classes](ArtClass* aClass)->bool {
        u4 typeIdx;
        GET_ART_CLASS_MEMBER(aClass,typeIdx,dex_type_idx_)
        auto realDexFile=getRealDexFile(aClass);
        classes.push_back(realDexFile->stringFromType(typeIdx));
        return false;
    }, artClass);
}

void* DexSeeker::getClass(JNIEnv *env, u4 classIdx)  {
    ClassMap::iterator iterator= classMap.find(classIdx);
    if(iterator == classMap.end()){
        const char* className= dexGlobal.dexFile->stringFromType(classIdx);
        if (className[0] != 'L' && className[0] !=
                                   '[') {//Array type is namely sub-type of object,so inherit all the virtual methods of object
            LOGE("Invalid class Found=%s  ", className);
            return nullptr;
        }
        LOGV("Class Name=%s",className);
        JavaString ClassName(toJavaClassName(className));
        jstring javaClassName = env->NewString(&ClassName[0],ClassName.Count());
        jobject firstInit=env->CallStaticObjectMethod(dexGlobal.getToolsClass(),dexGlobal.getFirstInitID(),javaClassName);
        if(firstInit== nullptr){
            return nullptr;
        }
        jmethodID methodId=env->FromReflectedMethod(firstInit);
        env->DeleteLocalRef(javaClassName);
        env->DeleteLocalRef(firstInit);

        if(methodId== nullptr){
            if(env->ExceptionCheck()==JNI_TRUE){
                env->ExceptionDescribe();
                env->ExceptionClear();
            }
            return nullptr;
        }
        if(isDalvik()){
            std::pair<u4,u4 *> pair(classIdx, (u4*)&reinterpret_cast<dalvik::Method*>(methodId)->clazz);
            classMap.insert(pair);
            return pair.second;
        } else{
            u4* classHandle;
            GET_ART_METHOD_MEMBER_PTR(classHandle,declaring_class_,methodId);
            if(*classHandle== 0)
                return nullptr;
            //u4 typeIndex;
           // ArtClass* artClass= reinterpret_cast<ArtClass*>(static_cast<uintptr_t >(*classHandle));
            //GET_ART_CLASS_MEMBER(artClass,typeIndex,dex_type_idx_)
            //const char * test=getRealDexFile(artClass)->stringFromType(typeIndex);
            std::pair<u4, u4 *> pair(classIdx, classHandle);
            classMap.insert(pair);
            return reinterpret_cast<void *>(*classHandle);
        }
    } else{
        return (void*) static_cast<uintptr_t >(*iterator->second);
    }
}
void checkIndex(int limit,u4 cur){
    if(limit<=cur){
        throw std::runtime_error(formMessage("index out of range,limit=",limit,"cur=",cur));
    }
}



u4 DexSeeker::getMethodByIndex(JNIEnv* env, u4 classIdx, u4 vIdx) {
    lock.lock();
    void * clazz=getClass(env, classIdx);
    lock.unlock();
    if(clazz== nullptr){
        LOGV("can't find class m");
        return UNDEFINED;
    }
    std::vector<const char*> classes;
    const char* name;
    const char* retType;
    std::string proto;
    if(isDalvik()){
        dalvik::ClassObject* classObject= (dalvik::ClassObject *) clazz;
        checkIndex(classObject->vtableCount,vIdx);
        dalvik::Method* method=classObject->vtable[vIdx];
        dalvik::DexFile* pDexFile= getDalvikDexFile(classObject);
        scanDalvikClassList([&classes](dalvik::ClassObject* object)-> bool{
            classes.push_back(object->descriptor);
            return false;
        },classObject);

        const dalvik::DexProtoId *dexProtoId=dalvik::dexGetProtoId(pDexFile, method->prototype.protoIdx);
        const dalvik::DexTypeList* typeList=dalvik::dexGetProtoParameters(pDexFile,dexProtoId);
        for(u4 i=0;i<typeList->size;++i){
            proto+=dalvik::dexStringByTypeIdx(pDexFile,dalvik::dexTypeListGetIdx(typeList,i));
        }
        name=method->name;
        retType= dalvik::dexStringByTypeIdx(pDexFile,dexProtoId->returnTypeIdx);
    } else{
        ArtClass* artClass=(ArtClass*) clazz;
        uintptr_t method=0;
        u4 vTableArray;
        GET_ART_CLASS_MEMBER(artClass,vTableArray,vtable_)
        if(vTableArray!=0){
#define ARRAY_GET(TYPE,INDEX,OUT,ARRAY_POINTER)\
            TYPE* array = reinterpret_cast<TYPE*>(ARRAY_POINTER);\
            checkIndex(array->size,INDEX);\
            OUT = array->elements[INDEX];

            if(dexGlobal.sdkOpt<ART_MARSHMALLOW){
                ARRAY_GET(ObjectArray,vIdx,method,vTableArray);
            } else{
                ARRAY_GET(PointerArray,vIdx,method,vTableArray)
            }
        } else{//just in case so don't care about its efficiency
            scanArtClassList([&method,vIdx](ArtClass* aClass)->bool{
                switch(dexGlobal.sdkOpt){
                    case ART_NOUGAT:{
                        NougatClass* nougatClass= reinterpret_cast<NougatClass*>(aClass);
                        u2 virtualMethodOffset= nougatClass->virtual_methods_;
                        u2 copiedMethodOffset=nougatClass->copied_methods_offset_;
                        art::Array<ArtMethodNougat>* array= reinterpret_cast<art::Array<ArtMethodNougat>*>(nougatClass->methods_);
                        if(array== nullptr) return false;
                        for(u4 i=virtualMethodOffset;i<copiedMethodOffset;++i){
                            ArtMethodNougat& artMethod=array->At(i);
                            if(artMethod.method_index_==vIdx){
                                method= reinterpret_cast<uintptr_t>(&artMethod);
                                return true;
                            }
                        }
                        return false;
                    }
                    default:{
                        u8 virtual_methods;
                        GET_ART_CLASS_MEMBER(aClass,virtual_methods,virtual_methods_)
                        if(dexGlobal.sdkOpt<ART_MARSHMALLOW){
                            ObjectArray* vArray= reinterpret_cast<ObjectArray*>(virtual_methods);
                            if(vArray== nullptr) return false;
                            for(int i=0;i<vArray->size;++i){
                                u4 methodPtr= vArray->elements[i];
                                if(methodPtr== 0) continue;
                                u4 methodIndex;
                                GET_ART_METHOD_MEMBER_VALUE(methodIndex,method_index_,methodPtr)
                                if(methodIndex==vIdx){
                                    method=methodPtr;
                                    return true;
                                }
                            }
                            return false;
                        } else{
                            ArtMethodMarshmallow* array=reinterpret_cast<ArtMethodMarshmallow*>(virtual_methods);
                            MarClass* thizClass=(MarClass*) aClass;
                            if(array== nullptr) return false;
                            for(u4 i=0;i<thizClass->num_virtual_methods_;++i){
                                ArtMethodMarshmallow& artMethod=array[i];
                                if(artMethod.method_index_==vIdx){
                                    method= reinterpret_cast<uintptr_t>(&artMethod);
                                    return true;
                                }
                            }
                            return false;
                        }
                    }
                }

            },artClass);
        }

        if(method==0){
            LOGV("Can't find a method");
            return UNDEFINED;
        }
        u4 methodIdIndex;u8 deCl;
        GET_ART_METHOD_MEMBER_VALUE(methodIdIndex,dex_method_index_,method);
        GET_ART_METHOD_MEMBER_VALUE(deCl,declaring_class_,method)
        auto dexFile=getRealDexFile((ArtClass*) deCl);
        auto& methodId=dexFile->method_ids_[methodIdIndex];
        addArtClassList(classes, artClass);
        auto &protoId = dexFile->proto_ids_[methodId.proto_idx_];
        getProtoString(protoId, dexFile, proto);
        name=dexFile->stringByIndex(methodId.name_idx_);
        retType=dexFile->stringFromType(protoId.return_type_idx_);
    }
    for(auto className:classes){
        LOGV("Start search method %s %s(%s)%s",className,name,&proto[0],retType);
        u4 ret=binarySearchMethod(className,name,retType,&proto[0]);
        if(ret!=UNDEFINED) return ret;
    }
    return UNDEFINED;
}

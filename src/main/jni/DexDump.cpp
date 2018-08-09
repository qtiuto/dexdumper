//
// Created by asus on 2016/7/18.
//

#include <algorithm>
#include "DexDump.h"
#include "util/checksum.h"
#include <dirent.h>
#include "dalvik/Object.h"
#include "CodeResolver.h"
#include "util/PtrVerify.h"
#include "util/DexUtils.h"
#include "util/SigCatcher.h"
#include <setjmp.h>
#include <sys/system_properties.h>
#include <cstring>

const char fill[] ="\0\0\0\0\0\0\0\0";
DexGlobal dexGlobal;
JavaVM *javaVM;
static sigjmp_buf nativeCrashJump;

void test();

static const JNINativeMethod dexDumpMethods[]={
        {"setMode","(I)V",(void*)setMode},
        {"dumpDexV16","(JLjava/lang/String;)V",(void*)dumpDexV16},
        {"dumpDexV19ForArt","(JLjava/lang/String;)V",(void*)dumpDexV19ForArt},
        {"dumpDexV21","(JLjava/lang/String;)V",(void*)dumpDexV21},
        {"dumpDexV23","([JLjava/lang/String;)V",(void*)dumpDexV23}
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved){
    log_init();
    atexit(log_end);
    JNIEnv* env;
    if(vm->GetEnv((void**)&env,JNI_VERSION_1_4)!=JNI_OK){
        return JNI_ERR;
    }
    javaVM=vm;
    jclass  dexDumpClass=env->FindClass("com/oslorde/extra/DexDumper");
    if(env->RegisterNatives(dexDumpClass,dexDumpMethods,5)<0){
        throw std::runtime_error("DexDumper methods native register failed");
    }
    LOGV("Vm putted");
    return JNI_VERSION_1_4;
}
JNIEXPORT  void JNICALL setMode(JNIEnv *env, jclass thisClass, jint mode){
    dexGlobal.dumpMode = (u1) mode;
    u1 size=(u1)((u4)mode>>24);
    if(size!=0){
        dexGlobal.poolSize=size;
    }
    char sdkStr[4];
    __system_property_get("ro.build.version.sdk",sdkStr);
    int sdk=atoi(sdkStr);
    LOGV("SDK=%d",sdk);
    if(sdk<21){
        dexGlobal.sdkOpt=DALVIK;
    } else{
        switch (sdk){
            case 21:
                dexGlobal.sdkOpt=ART_LOLLIPOP;
            case 22:
                dexGlobal.sdkOpt=ART_LOLLIPOP_MR1;
            case 23:
                dexGlobal.sdkOpt=ART_MARSHMALLOW;
            case 24:
            case 25:
                dexGlobal.sdkOpt=ART_NOUGAT;
            default:
                dexGlobal.sdkOpt=ART_NOUGAT;
        }
    };
}

JNIEXPORT  void JNICALL dumpDexV16(JNIEnv *env, jclass thisClass,
                                   jlong cookie,
                                   jstring baseOutDir) {
    dalvik::DexOrJar* pDexOrJar = (dalvik::DexOrJar*) cookie;
    dalvik::DvmDex* pDvmDex;
    if(pDexOrJar->isDex){
        pDvmDex=pDexOrJar->pRawDexFile->pDvmDex;
    } else pDvmDex=pDexOrJar->pJarFile->pDvmDex;
    LOGV("Dex1=%p,Dex2=%p", pDvmDex, pDvmDex->pDexFile);
    dalvik::DexFile* dex_file=pDvmDex->pDexFile;
    dalvik::DexFile* dexFile=new dalvik::DexFile;
    memcpy(dexFile, dex_file, sizeof(dalvik::DexFile));
    const dalvik::DexHeader* header1= pDvmDex->pHeader;
    const dalvik::DexHeader* header2=dexFile->pHeader;
    bool isF=true;
    if(header1!=header2){
        LOGW("Header unequal,check it,mag1=%s,mag2=%s", header1->magic, header2->magic);
        if(memcmp(header1->magic,header2->magic,8)<0){
            isF=false;
        }
        if(header1->checksum==0) isF= false;
        if(strlen((const char*)header1->signature)==0) isF= false;
        if(header1->stringIdsOff==0) isF= false;
        if(header1->classDefsOff==0) isF= false;
        if(header1->methodIdsOff==0) isF= false;
    }
    if(isF) dexFile->pHeader=header1;
    art::DexFile dex(dexFile->baseAddr,
                     (const art::DexFile::Header *const) dexFile->pHeader,
                     (const art::DexFile::StringId *const) dexFile->pStringIds,
                     (const art::DexFile::TypeId *const) dexFile->pTypeIds,
                     (const art::DexFile::FieldId *const) dexFile->pFieldIds,
                     (const art::DexFile::MethodId *const) dexFile->pMethodIds,
                     (const art::DexFile::ProtoId *const) dexFile->pProtoIds,
                     (const art::DexFile::ClassDef *const) dexFile->pClassDefs);
    std::vector<const art::DexFile*> dex_files;
    dex_files.reserve(1);
    dex_files.push_back(&dex);
    jboolean isCopy ;
    const char* outDir= env->GetStringUTFChars(baseOutDir, &isCopy );
    CodeResolver::resetInlineTable();
    dumpDex(env,dex_files,outDir);
    // ReleaseStringUTFChars can be called with an exception pending.
    env->ReleaseStringUTFChars(baseOutDir, outDir);
    delete dexFile;
}
JNIEXPORT  void JNICALL dumpDexV19ForArt(JNIEnv *env, jclass thisClass, jlong cookie,
                                         jstring baseOutDir){
    using namespace art;
    dexGlobal.sdkOpt=ART_KITKAT;
    const DexFile* dexFile= reinterpret_cast<DexFile*>(cookie);
    std::vector<const art::DexFile*> dex_files;
    dex_files.reserve(1);
    dex_files.push_back(dexFile);
    jboolean isCopy ;
    const char* outDir= env->GetStringUTFChars(baseOutDir, &isCopy );
    dumpDex(env,dex_files,outDir);
    // ReleaseStringUTFChars can be called with an exception pending.
    env->ReleaseStringUTFChars(baseOutDir, outDir);
}
JNIEXPORT  void JNICALL dumpDexV21(JNIEnv *env, jclass thisClass, jlong dex_file_address,
                                   jstring baseOutDir){
    std::vector<const art::DexFile*>* dex_files = reinterpret_cast<std::vector<const art::DexFile*>*>(
		static_cast<uintptr_t>(dex_file_address));
	jboolean isCopy ;
	const char* outDir=env->GetStringUTFChars(baseOutDir, &isCopy );
	dumpDex(env,dex_files[0], outDir);
    // ReleaseStringUTFChars can be called with an exception pending.
	env->ReleaseStringUTFChars(baseOutDir, outDir);
}
JNIEXPORT  void JNICALL dumpDexV23(JNIEnv *env, jclass thisClas, jlongArray dex_files_address,
                                   jstring baseOutDir){
	jsize array_size = env->GetArrayLength(dex_files_address);
	if (env->ExceptionCheck() == JNI_TRUE) {
		return;
	}
	jboolean is_long_data_copied;
	jlong* long_data = env->GetLongArrayElements(dex_files_address,
		&is_long_data_copied);
	if (env->ExceptionCheck() == JNI_TRUE) {
		return ;
	}
	std::vector<const art::DexFile*> ret;
    ret.reserve((unsigned long) array_size);
	for (jsize i = 0; i < array_size; ++i) {
		ret.push_back(reinterpret_cast<const art::DexFile*>(static_cast<uintptr_t>(*(long_data + i))));
	}
	env->ReleaseLongArrayElements(reinterpret_cast<jlongArray>(dex_files_address), long_data, JNI_ABORT);
	if (env->ExceptionCheck() == JNI_TRUE) {
		return ;
	}
	jboolean isCopy;
	const char* outDir = env->GetStringUTFChars(baseOutDir, &isCopy);
	dumpDex(env,ret, outDir);
    // ReleaseStringUTFChars can be called with an exception pending.
	env->ReleaseStringUTFChars(baseOutDir, outDir);
}

static void nativeCrashHandler(int sig) {
    siglongjmp(nativeCrashJump, 1);
}

static void dumpDex(JNIEnv* env,std::vector<const art::DexFile*>& dex_files,const char* outDir){
    //void* func=dlsym(RTLD_DEFAULT,"dumpDexV19ForArt");
    //LOGV("Found=%p,real=%p",func,dumpDexV19ForArt);
    if (dexGlobal.isThrowToJava()) {
        LOGV("Is Throw to java");
        struct sigaction sa[2], osa[2];
        sa[0].sa_handler = nativeCrashHandler;
        memset(&sa[0].sa_mask, 0, sizeof(sa[0].sa_mask));
        sa[0].sa_flags = 0;
        sigaction(SIGABRT, &sa[0], &osa[0]);
        sa[1] = sa[0];
        sigaction(SIGSEGV, &sa[1], &osa[1]);
        if (sigsetjmp(nativeCrashJump, 1) != 0) {
            sigaction(SIGABRT,&osa[0], nullptr);
            sigaction(SIGSEGV, &osa[1], nullptr);
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Oops!Native Crashed");
            return;
        }
    }
    using namespace art;
    dexGlobal.setToolsClass(env);
    /*dexGlobal.setupDexSeeker();
    dexGlobal.dexFile=dex_files[0];
    u4 mid=dexGlobal.dexSeeker->getMethodByIndex(env,16,0);
    LOGV("Method Index=%d",mid);
    //DexFile* dexFile=getRealDexFile(reinterpret_cast<ArtClass*>(dexGlobal.dexSeeker->getClass(env,16)));
    //logMethod(dexGlobal.dexFile->method_ids_[mid],dexGlobal.dexFile);
    return;*/

	int dirLen = (int) strlen(outDir);
    char dexFileName[dirLen + 23];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"
    dexGlobal.dexFileName = dexFileName;//only within this method
#pragma clang diagnostic pop



    memset(dexFileName, '\0', (size_t) (dirLen + 23));
    memcpy(dexFileName, outDir, (size_t) dirLen);
    if(outDir[dirLen-1]!='/'){
        dexFileName[dirLen++] = '/';
    }
    int count=0;
    struct dirent *dp;
    DIR *dirp = opendir(dexFileName); //打开目录指针
    while ((dp = readdir(dirp)) != NULL) { //通过目录指针读目录
        char* fileName=dp->d_name;
        int num= (int) (strlen(fileName) - 4);
        if(num<0)continue;
        if(strcmp(fileName+num,".dex")!=0)
            continue;
        char name[num+1];
        name[num]=0;
        memcpy(name, fileName, (size_t) num);
        num = parsePositiveDecimalInt(name);
        if(num>count) count=num;
    }
    closedir(dirp);
    LOGV("Dir set");
    for (const DexFile* dex : dex_files) {
        if(dex== nullptr){
            LOGW("Dex ptr is null");
            continue;
        }
		++count;
        setDexName(count, dexFileName + dirLen);
        LOGV("Dex out %s", dexFileName);
        int fd = open(dexFileName, O_RDWR | O_CREAT, 00700);
        if(fd==-1){
            LOGE("Open File: %s Failed", dexFileName);
            continue;
        } else{
            struct stat buf;
            if(fstat(fd,&buf)==-1){
                perror("Oslorde_dexdumper:Can not read or write the dex file");
                continue;
            }
        }
        dexGlobal.dexFile=dex;
        LOGV("dexFile=%p will be written to %s", dex, dexFileName);
        if(dexGlobal.pool!= nullptr)
            dexGlobal.pool->reOpen();
		std::vector<::DataSection*> dataSection;
        HeadSection heads[7];
        LinkData* linkData= nullptr;

        u1* begin = const_cast<u1*>(dex->begin_);


		DexFile::Header header =*(dex->header_) ;
		if (memcmp(header.magic_, "dex\n",4) != 0 ||!judgeVersion(&header.magic_[4])) {
			LOGV("Invalid magic version %s",header.magic_);
			memcpy(header.magic_,reinterpret_cast<const uint8_t*>("dex\n035"), 8);
		}
		header.endian_tag_ = 0x12345678;//Only accept little endian;
        if(header.link_size_>0){
            LOGV("link size %d",header.link_size_);
            linkData=new LinkData;
            linkData->size=header.link_size_;
            linkData->data= const_cast<u1*>(reinterpret_cast<const u1*>(header.link_off_+begin)) ;
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"
		heads[0].ptr= (u1 *)& header;
#pragma clang diagnostic pop
        heads[0].num = 1;
        heads[0].size= sizeof(DexFile::Header);
        header.header_size_=sizeof(DexFile::Header);


        char buf[sizeof(DexFile::MapItem)*18/*the max num of size*/ + 4U];
        DexFile::MapList* mapList=new(buf) DexFile::MapList;
        DataSection* mapSection=new DataSection;
        mapSection->type=typeMapList;
        mapSection->src=mapList;//Don't use ptr directly add or substract unless it's u1* or char*
        mapSection->size=0;// to be fix in the nature
        dataSection.push_back(mapSection);//mapof will be updated directly on header;


        header.string_ids_off_= sizeof(DexFile::Header);
        const DexFile::StringId* stringIds = dex->string_ids_;
        u4 idNum = header.string_ids_size_;
        heads[1].num =  idNum;
        heads[1].size= (u4) (idNum * sizeof(DexFile::StringId));
        heads[1].ptr= (u1 *) stringIds;
		for (int i = 0;i < idNum;++i) {
			const DexFile::StringId& stringId =  *(stringIds + i);
            int size;const u1* ptr=reinterpret_cast<const uint8_t*>(begin + stringId.string_data_off_);
            DataSection* stringData=new DataSection;
            stringData->src =ptr;
			readUnsignedLeb128(size,ptr);
            stringData->type = typeStringDataItem;
			stringData->size= (u4) size;
            size= (int) (strlen((const char*) ptr) + 1);//count the encoded length of the string,don't use the return size value as it's the decoded length.
            stringData->size+=size;
            stringData->parStart=header.string_ids_off_;
			stringData->parOffset =  (i * (u4)sizeof(DexFile::StringId));
			dataSection.push_back(stringData);
		}
        header.type_ids_off_=header.string_ids_off_+heads[1].size;
		const DexFile::TypeId* typeIds = dex->type_ids_;
		idNum = header.type_ids_size_;
        heads[2].ptr= (u1 *) typeIds;
        heads[2].num = idNum;
        heads[2].size=idNum*(u4) sizeof(DexFile::TypeId);

        header.proto_ids_off_=header.type_ids_off_+heads[2].size;
		const DexFile::ProtoId* protoIds = dex->proto_ids_;
		idNum = header.proto_ids_size_;
        heads[3].num = idNum;
        heads[3].ptr= (u1 *) protoIds;
        heads[3].size=idNum*(u4) sizeof(DexFile::ProtoId);
		for (int i = 0;i < idNum;++i) {
            DexFile::ProtoId& id = const_cast<DexFile::ProtoId&>(*(protoIds + i));
			if (id.parameters_off_ == 0) continue;//no parameter
			putTypeList(dataSection, id.parameters_off_,i*(u4) sizeof(DexFile::ProtoId) +
                    header.proto_ids_off_, offsetof(DexFile::ProtoId,parameters_off_),typeParameter, begin);
		}

        header.field_ids_off_=header.proto_ids_off_+heads[3].size;
		const DexFile::FieldId* fieldIds = dex->field_ids_;
		idNum = header.field_ids_size_;
        heads[4].ptr= (u1 *) fieldIds;
        heads[4].num = idNum;
        heads[4].size=idNum* (u4)sizeof(DexFile::FieldId);

        header.method_ids_off_=header.field_ids_off_+heads[4].size;
		const DexFile::MethodId* methodIds = dex->method_ids_;
		idNum = header.method_ids_size_;
        heads[5].ptr= (u1 *) methodIds;
        heads[5].num =  idNum;
        heads[5].size=idNum*(u4) sizeof(DexFile::MethodId);

        header.class_defs_off_=header.method_ids_off_+heads[5].size;
		const DexFile::ClassDef* classDefs = dex->class_defs_;
        idNum = header.class_defs_size_;
        heads[6].num = idNum;
        heads[6].size= idNum * (u4) sizeof(DexFile::ClassDef);
        heads[6].ptr= (u1 *) classDefs;


        jclass toolsClass =dexGlobal.getToolsClass();
        jmethodID findMethod=env->GetStaticMethodID(toolsClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        if(env->ExceptionCheck()==JNI_TRUE){
            LOGE("Can't find findClass() check why");
            env->ExceptionClear();
            return;
        }
        header.data_off_=header.class_defs_off_+heads[6].size;
        fixMapListHeaderPart(&header, heads, mapList);//mapList second
        writeHeadSection(heads,fd);

        LOGV("Start resolving class def");
		for (int i = 0; i < idNum; ++i) {
            
            const DexFile::ClassDef &clsDefItem = *(classDefs + i);
            const char* clsChars= dex->stringFromType(clsDefItem.class_idx_);
            //LOGV("Start put cls data,cls name:%s,classIdx=%u",clsChars,clsDefItem.class_idx_);
            jclass thizClass= nullptr;
            if(isFixCode()){
                JavaString ClassName(toJavaClassName(clsChars));
                jstring javaClassName = env->NewString(&ClassName[0],ClassName.Count());//skip primitive class
                thizClass = (jclass) env->CallStaticObjectMethod(toolsClass, findMethod, javaClassName);
                if(thizClass==NULL){
                    thizClass= nullptr;
                    LOGW("Class %s can't be init in jni environment,\n"
                                 "may be they are rejected for referring "
                                 "to inaccessible class,e.g. xposedbridge.", clsChars);
                    LOGW("So,dynamic code fix is disabled for this class");
                }
                env->DeleteLocalRef(javaClassName);
            }
			if (clsDefItem.interfaces_off_ != 0){
                putTypeList(dataSection, clsDefItem.interfaces_off_,i* (u4)sizeof(DexFile::ClassDef)
                           +header.class_defs_off_, offsetof(DexFile::ClassDef
                ,interfaces_off_), typeInterface, begin);
                
            }
			if (clsDefItem.annotations_off_ != 0) {
				 DexFile::AnnotationsDirectoryItem* directory = reinterpret_cast<DexFile::AnnotationsDirectoryItem*>(begin + clsDefItem.annotations_off_) ;
				//LOGV("AnnotationDir not null,ptr=%p",directory);
				DataSection* section = new DataSection;
				section->type = typeAnnotationDirectoryItem;
				section->size =static_cast<u4>(sizeof(DexFile::AnnotationsDirectoryItem)+ directory->fields_size_*sizeof(DexFile::FieldAnnotationsItem) +
					directory->methods_size_*sizeof(DexFile::MethodAnnotationsItem) + directory->parameters_size_*sizeof(DexFile::ParameterAnnotationsItem));
                section->parOffset=offsetof(DexFile::ClassDef,annotations_off_);
                section->parStart=i* (u4)sizeof(DexFile::ClassDef)
                                  +header.class_defs_off_;
                section->src =  directory ;

				dataSection.push_back(section);
                u1*ptrCur = reinterpret_cast<u1*>(directory+ 1U);//as directory is of that type so just offset 1
                u1* ptrStart= (u1 *) directory;
                u4 classAnnoOffset=directory->class_annotations_off_;
                if(classAnnoOffset!=0){
                    DexFile::AnnotationSetItem* setItem= reinterpret_cast<DexFile::AnnotationSetItem*>(begin+classAnnoOffset);
                    putAnnoSetItem(dataSection, section, 0, setItem, begin);
                }
				if (directory->fields_size_>0) {

					for (int j = 0;j < directory->fields_size_;++j) {
						DexFile::FieldAnnotationsItem* item = reinterpret_cast<DexFile::FieldAnnotationsItem*>(ptrCur);
                        DexFile::AnnotationSetItem* setItem= reinterpret_cast<DexFile::AnnotationSetItem*>(begin+item->annotations_off_);
                        putAnnoSetItem(dataSection, section, (u4) (ptrCur - ptrStart) +offsetof(
                                DexFile::FieldAnnotationsItem,annotations_off_), setItem, begin);
						ptrCur += sizeof(DexFile::FieldAnnotationsItem);
					}

				}
				if (directory->methods_size_>0) {
					for (int j = 0;j < directory->methods_size_;++j) {
						DexFile::MethodAnnotationsItem* item = reinterpret_cast<DexFile::MethodAnnotationsItem*>(ptrCur);
                        DexFile::AnnotationSetItem* setItem= reinterpret_cast<DexFile::AnnotationSetItem*>(begin+item->annotations_off_) ;

                        putAnnoSetItem(dataSection, section, (u4) (ptrCur - ptrStart)+offsetof(
                                DexFile::MethodAnnotationsItem,annotations_off_), setItem, begin);
						ptrCur += sizeof(DexFile::MethodAnnotationsItem);
					}
				}
                if(directory->parameters_size_>0){
                    for(int j=0;j<directory->parameters_size_;++j){
                        DexFile::ParameterAnnotationsItem* item= reinterpret_cast<DexFile::ParameterAnnotationsItem*>(ptrCur);
                        DexFile::AnnotationSetRefList* list= reinterpret_cast<DexFile::AnnotationSetRefList*>(item->annotations_off_+begin);
                        DataSection*sect =new DataSection;
                        sect->type=typeAnnotationSetRefList;
                        sect->size=4U+int(list->size_ * sizeof(DexFile::AnnotationSetRefItem));
                        sect->src=list;
                        sect->parRef=section;
                        sect->parOffset= (u4) (ptrCur - ptrStart)+offsetof(DexFile::ParameterAnnotationsItem,annotations_off_);
                        dataSection.push_back(sect);

                        for(u4 k=0;k<list->size_;++k){
                            DexFile::AnnotationSetRefItem* setRefItem=list->list_+k;
                            if(setRefItem->annotations_off_==0){
                                continue;
                            }
                            DexFile::AnnotationSetItem* setItem= reinterpret_cast<DexFile::AnnotationSetItem*>(begin+setRefItem->annotations_off_);
                            putAnnoSetItem(dataSection,sect,(k+1)<<2,setItem,begin);
                        }
                        ptrCur += sizeof(DexFile::ParameterAnnotationsItem);
                    }
                }
			}
            
            if(clsDefItem.class_data_off_ != 0){
                int size;
                const u1* ptr= begin + clsDefItem.class_data_off_;
                const u1* beginPtr=ptr;
                int staticFieldsSize=readUnsignedLeb128(size,ptr);
                int instanceFieldsSize=readUnsignedLeb128(size,ptr);
                int directMethodSize=readUnsignedLeb128(size,ptr);
                int virtualMethodSize=readUnsignedLeb128(size,ptr);
                //LOGV("Field:s=%d,i=%d; Method:d=%d,v=%d",staticFieldsSize,instanceFieldsSize,directMethodSize,virtualMethodSize);
                for(int j=0;j<staticFieldsSize+instanceFieldsSize;++j){
                    readUnsignedLeb128(size,ptr);//field_idx_diff
                    readUnsignedLeb128(size,ptr);//access_flags
                }
                int methodBeginSize=int(ptr-beginPtr);
                //LOGV("Method begin size=%d",methodBeginSize);
                for(int j=0;j<directMethodSize+virtualMethodSize;++j){
                    readUnsignedLeb128(size,ptr);//method_idx_diff
                    readUnsignedLeb128(size,ptr);//access_flags
                    readUnsignedLeb128(size,ptr);
                }
                
                size_t classSize = sizeof(ClassDataSection) + sizeof(ULebRef) * (virtualMethodSize + directMethodSize);
                char* dataBuf =new char[classSize];
                memset(dataBuf,0,classSize);
                
                ClassDataSection* section=new(dataBuf)ClassDataSection;
                
                section->size=u4(ptr-beginPtr);
                section->src=beginPtr;

                section->type=typeClassDataItem;
                section->parOffset=offsetof(DexFile::ClassDef,class_data_off_);
                section->parStart=header.class_defs_off_+i*sizeof(DexFile::ClassDef);
                
                dataSection.push_back((DataSection*)section);

                ptr=beginPtr+methodBeginSize;

                
                fixMethodCodeIfNeeded(env, dex, directMethodSize, thizClass, dataSection, ptr,
                                      section, dex_files);

                fixMethodCodeIfNeeded(env, dex, virtualMethodSize, thizClass, dataSection, ptr,
                                      section, dex_files);
                
            }
            if(clsDefItem.static_values_off_ != 0){
                DataSection* section=new DataSection;
                section->type=typeStaticValueItem;
                u1* ptr= begin + clsDefItem.static_values_off_;
                section->size= (u4) getEncodedArraySize(ptr);
                section->src= (clsDefItem.static_values_off_+begin);
                section->parOffset=offsetof(DexFile::ClassDef,static_values_off_);
                section->parStart=header.class_defs_off_+i*sizeof(DexFile::ClassDef);
                dataSection.push_back(section);
                
                //LOGV("Push Back Static Values size=%u",section->size);
            }
            if(thizClass!= nullptr){
                env->DeleteLocalRef(thizClass);
            }
		}
        
        LOGV("Begin fix");
        fixDataSection(dataSection, &header);
        u4 fileSize=header.data_off_+header.data_size_;
        if(linkData!= nullptr){
            header.link_off_=fileSize;
            fileSize+=linkData->size;
        }
        header.file_size_=fileSize;
        DexCacheFile dexCacheFile(fd, fileSize);
        dexCacheFile.pwrite(&header, sizeof(DexFile::Header), 0);
        LOGV("Start writing,data_off=%u,data_size=%u,file size=%u",header.data_off_,header.data_size_,fileSize);

        dexCacheFile.seek(header.data_off_);
        LOGV("Start write data Section pos=%u", dexCacheFile.tell());
		for (DataSection* section : dataSection) {
            writeDataSection(dexCacheFile, section);
		}
        if (dexCacheFile.tell() - header.data_off_ != header.data_size_) {
            LOGW("Write dex file wrong,some elements are "
                         "forgotten to be emplaced,now pos=%ld",lseek(fd,0,SEEK_CUR));
        }
        if(linkData!= nullptr){
            LOGV("Start writing link data");
            dexCacheFile.write((const char *) linkData->data, linkData->size);
            delete linkData;
        }
        LOGV("Update ref");
        for(DataSection* section:dataSection){
            updateRef(dexCacheFile, section);
        }
        dexCacheFile.flush();
        LOGV("delete data");
        for(DataSection* section:dataSection){
            if(section->type!=typeClassDataItem)
                delete section;
            else delete [] (char*)(section);
        }
        LOGV("Execute pending");
        if(dexGlobal.pool!= nullptr){
            dexGlobal.pool->executeAllPendingTasks();
            dexGlobal.pool->waitForFinish();
        }
        LOGW("Wait over!Start writing new header");
        //the most serious efficiency problems
        jstring dexPath = env->NewStringUTF(dexFileName);
        jmethodID hashMid=env->GetStaticMethodID(toolsClass, "getDexSHA1Hash", "(Ljava/lang/String;)[B");
        jbyteArray fileContent= (jbyteArray) env->CallStaticObjectMethod(toolsClass, hashMid, dexPath);

        env->DeleteLocalRef(dexPath);
        jbyte * readBuf= env->GetByteArrayElements(fileContent, nullptr);
        DexFile::Header* fileHeader= reinterpret_cast<DexFile::Header*>(readBuf);
        fileHeader->checksum_= adler32((char *) fileHeader->signature_, fileSize - 12);

        pwrite(fd,(const char*)fileHeader, sizeof(DexFile::Header),0);
        close(fd);
        env->ReleaseByteArrayElements(fileContent,readBuf,JNI_ABORT);
        env->DeleteLocalRef(fileContent);

        
        LOGV("One dex is over");
	}
    dexGlobal.release(env);
}

void test() {
    if(dexGlobal.pool!= nullptr&&dexGlobal.pool->isDirty()){
            LOGV("DIRTY");
    }
}

static bool judgeVersion(const unsigned char* str){
    if(strlen((const char *)str)!=3) return false;
    return parsePositiveDecimalInt((const char *) str) >= 35;//ealier version not supported as their platform are below android 4.0
}

static void fixMapListHeaderPart(const art::DexFile::Header *header, const HeadSection *const heads,
                                 art::DexFile::MapList *mapList){
    mapList->list_[0].type_=art::DexFile::kDexTypeHeaderItem;
    mapList->list_[0].offset_=0;
    mapList->list_[1].type_=art::DexFile::kDexTypeStringIdItem;
    mapList->list_[1].offset_=header->string_ids_off_;
    mapList->list_[2].type_=art::DexFile::kDexTypeTypeIdItem;
    mapList->list_[2].offset_=header->type_ids_off_;
    mapList->list_[3].type_=art::DexFile::kDexTypeProtoIdItem;
    mapList->list_[3].offset_=header->proto_ids_off_;
    mapList->list_[4].type_=art::DexFile::kDexTypeFieldIdItem;
    mapList->list_[4].offset_=header->field_ids_off_;
    mapList->list_[5].type_=art::DexFile::kDexTypeMethodIdItem;
    mapList->list_[5].offset_=header->method_ids_off_;
    mapList->list_[6].type_=art::DexFile::kDexTypeClassDefItem;
    mapList->list_[6].offset_=header->class_defs_off_;
    for(int i=0;i<7;++i)
        mapList->list_[i].size_= (u4) heads[i].num;
}

static void changeItemState(ItemState itemStates[],int index,DataSection *section,u4& size,u4 data_off,u2 type, bool requiredPadding){
    if(itemStates[index].isFirst){
        itemStates[index].isFirst= false;
        if(requiredPadding){
            u1 paddingRequired;//4 byte alignment
            if((paddingRequired= (u1) (4 - size % 4)) != 0 && paddingRequired < 4){
                section->prePaddingSize=paddingRequired;
                size+=paddingRequired;
            }
        }
        itemStates[index].offset=size+data_off;
        itemStates[index].type=type;
        //LOGV("Into type %s,size=%d",getDexDataTypeName(type),size);
    }
    ++(itemStates[index].size);
}

static void fixDataSection(std::vector<::DataSection *> &dataSection, art::DexFile::Header *header) {
    //LOGV("Begin fix dataSection");
    /*std::vector<::DataSection *> dataCopy(dataSection);//avoid per section order being mixed
    std::sort(std::begin(dataCopy), std::end(dataCopy), compareOffset);//no need to maintain;
	bool shouldChangeDataOff = header->data_off_ != (*dataCopy.begin())->offset;
    DataSection* pre= nullptr;
    for(auto& data:dataCopy){
        if(pre!= nullptr){
            if((pre->offset+pre->size)>data->offset){
                LOGW("data overlap %s,%s", getDexDataTypeName(pre->type), getDexDataTypeName(data->type));
            }
            pre=data;
        }
    }*/
    std::stable_sort(dataSection.begin(), dataSection.end(), compareDexType);//Don't change per section order;
    insertEmptySections(dataSection);

    u4 size=0;u4 data_off=header->data_off_;
    u1 paddingRequired;
    ItemState states[12];

    int index;
    //LOGV("Start re-count data mapping");
    for (DataSection *&section : dataSection) {
        section->prePaddingSize=0;
        section->postPaddingSize=0;
        index=section->type;
        switch (section->type){
            case typeAnnotationSetRefList:{
                changeItemState(states,index,section,size,data_off,section->type, false);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else{
                    states[index].isFirst= true;
                }
                size+=section->size;
                break;
            }
            case typeAnnotationSetItem:{
                changeItemState(states,index,section,size,data_off,section->type, false);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else{
                    states[index].isFirst= true;
                }
                size+=section->size;
                break;
                //4 byte alignment,but the above item is 4 byte alignment
            }
            case typeCodeItem:{
                changeItemState(states,index,section,size,data_off,section->type, true);
                if(section->src!=nullptr){
                    CodeItemSect* codeSect= static_cast<CodeItemSect*>(section);
                    u4 curOffset=data_off+size;
                    codeSect->updateFileOffset(curOffset);
                    int curRefSize=unsignedLeb128Size(curOffset);
                    ClassDataSection* classData=(ClassDataSection*) codeSect->parRef;
                   classData->codeRefs[codeSect->parIdx].nowSize= (u1) curRefSize;

                }else{
                    states[index].isFirst= true;
                    //LOGV("nullptr found, keep first,type=%s",getDexDataTypeName(states[index].type));
                }
                if((paddingRequired= (u1) (4 - section->size % 4)) != 4){
                    section->postPaddingSize=paddingRequired;
                    size+=paddingRequired;//4 byte alignment
                }
                size+=section->size;
                break;
            }
            case typeAnnotationDirectoryItem:{
                changeItemState(states,index,section,size,data_off,section->type, true);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else{
                    states[index].isFirst= true;
                    //LOGV("nullptr found, keep first,type=%s",getDexDataTypeName(states[index].type));
                }
                if((paddingRequired= (u1) (4 - section->size % 4)) != 4){
                    section->postPaddingSize=paddingRequired;
                    size+=paddingRequired;//4 byte alignment
                }
                size+=section->size;
                break;
            }
            case typeInterface:{
                changeItemState(states,index,section,size,data_off,section->type, true);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else{
                    states[index].isFirst= true;
                }
                if((paddingRequired= (u1) (4 - section->size % 4)) !=  4){
                    section->postPaddingSize=paddingRequired;
                    size+=paddingRequired;//4 byte alignment
                }
                size+=section->size;
                break;
            }
            case typeParameter:{
                changeItemState(states,index,section,size,data_off,section->type, true);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else{
                    states[index].isFirst= true;
                }
                if((paddingRequired= (u1) (4 - section->size % 4)) !=  4){
                    section->postPaddingSize=paddingRequired;
                    size+=paddingRequired;//4 byte alignment
                }
                size+=section->size;
                break;
            }
            case typeStringDataItem:{
                changeItemState(states,index,section,size,data_off,section->type, false);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else states[index].isFirst= true;
                size+=section->size;
                break;
            }
            case typeDebugInfoItem:{
                changeItemState(states,index,section,size,data_off,section->type, false);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else states[index].isFirst= true;
                size+=section->size;
                break;
            }
            case typeAnnotationItem:{
                changeItemState(states,index,section,size,data_off,section->type, true);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else states[index].isFirst= true;
                size+=section->size;//no alignment required
                break;
            }
            case typeStaticValueItem:{
                changeItemState(states,index,section,size,data_off,section->type, true);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else states[index].isFirst= true;
                size+=section->size;
                break;
            }
            case typeClassDataItem:{
                changeItemState(states,index,section,size,data_off,section->type, false);
                if(section->src!=nullptr){
                    section->fileOffset=data_off+size;
                    size+=section->size;
                    for(int i=0;i<((ClassDataSection*)section)->codeRefSize;++i){
                        ULebRef& ref=((ClassDataSection*)section)->codeRefs[i];
                        size=size-ref.origSize+ref.nowSize;
                    }
                }
                else states[index].isFirst= true;

                break;
            }
            case typeMapList:{
                using namespace art;
                DexFile::MapList* mapList= (DexFile::MapList*)(section->src);
                u4 mapCount=7;//skip header fields
                for(int i=0;i<11;++i)
                    fillMapItem(&mapList->list_[mapCount],states,i,mapCount);
                DexFile::MapItem* listItem=&mapList->list_[mapCount++];
                mapList->size_=mapCount;
                listItem->size_=1;//map list item has only one
                section->size=(u4) (sizeof(DexFile::MapItem) * mapCount + 4U);
                listItem->unused_=0;
                listItem->type_=DexFile::kDexTypeMapList;
                if((paddingRequired= (u1) ((size + section->size) % 8)) != 0){
                    paddingRequired= (u1) (8 - paddingRequired);
                    size+=paddingRequired;
                    section->prePaddingSize=paddingRequired;
                }
                listItem->offset_=data_off+size;
                header->map_off_=data_off+size;
                size+=section->size;
            }
            default:{
                break;
            }
        }
    }
    header->data_size_=size;
}
static void fillMapItem(art::DexFile::MapItem* mapItem,ItemState state[],int& index,u4 &mapCount){
    if(state[index].isFirst) return;
    mapItem->type_= (uint16_t) getMapItemTypeFromDataType(state[index].type);
    mapItem->offset_=state[index].offset;
    mapItem->size_=state[index].size;
    mapItem->unused_=0;
    ++mapCount;
    if(state[index].type==typeParameter){
        LOGW("Error occured,unexpected type:typeParameter,may be there's no interface,check it!");
    }
    if(state[index].type==typeInterface){
        ++index;
        mapItem->size_+=state[index].size;
    }

}
static void insertEmptySections(std::vector<::DataSection *> &dataSection){

    int preType=-1 ,lastType=-1;u2 curType;
    for(size_t i=0,len=dataSection.size();i<len;++i){
        DataSection* sect=dataSection.at(i);
        if(sect== nullptr){
            LOGV("Unexpected Null Sect");
        }
         curType=sect->type;
        if(lastType!=curType) {
            preType=lastType;
        }
        if(curType-preType>=2){
            while((++preType)<curType){
                DataSection* empty=new DataSection();
                empty->type= (u2) preType;
                dataSection.push_back(empty);
                LOGV("Inserted Section:%s",getDexDataTypeName((u2)preType));
            }
        }
        lastType=curType;
    }
    std::stable_sort(dataSection.begin(), dataSection.end(), compareDexType);//Don't change type order;
    //usleep(500);
}

bool compareDexType( DataSection*const &first, DataSection*const &sec){
    return first->type<sec->type;
}

static void writeHeadSection(const HeadSection head[7], int fd){
    //lseek(fd,0,SEEK_SET);
    for(int i=0;i<7;++i){
        const HeadSection& section=head[i];
        const char* ptr= reinterpret_cast<const char *>(section.ptr);
        write(fd,ptr,section.size);
    }
}


static void writeDataSection(DexCacheFile &dexCacheFile, DataSection *section) {

    dexCacheFile.write(fill, section->prePaddingSize);
    if(section->type!=typeClassDataItem){
        const uint32_t offset = dexCacheFile.tell();
        if (offset != section->fileOffset && section->fileOffset != 0) {
            LOGE("Offset off type %s mismatch", getDexDataTypeName(section->type));
        }
        dexCacheFile.write(section->src, section->size);
        if (section->type == typeCodeItem &&section->size>0/*EMPTY ONE*/&&
            !static_cast<CodeItemSect *>(section)->isValidDebugOff) {
            LOGV("Meet bad code Item");
            art::DexFile::CodeItem *codeItem = (art::DexFile::CodeItem *) dexCacheFile.getCache(
                    offset);
            codeItem->debug_info_off_ = 0;
        }
    }
    else if(section->size>0){
        ClassDataSection* classData= static_cast<ClassDataSection*>(section);
        u1 * ptr= (u1 *) section->src;int offset =0;
        for(int i=0;i<classData->codeRefSize;++i){
            ULebRef& ref=classData->codeRefs[i];
            dexCacheFile.write(ptr + offset, ref.offset - offset);
            offset = ref.offset + ref.origSize;
            ref.offset = dexCacheFile.tell();
            dexCacheFile.offset(ref.nowSize);
        }
        dexCacheFile.write(ptr + offset, classData->size - offset);
    }
    dexCacheFile.write(fill, section->postPaddingSize);
}

static void updateRef(DexCacheFile &dexCacheFile, DataSection *section) {
    if(section->fileOffset== 0) return;
    u4 offsetData;
    switch (section->type){
        case typeMapList:
            return;//no need
        case typeAnnotationSetItem:
        case typeAnnotationSetRefList:
        case typeAnnotationItem:
        case typeDebugInfoItem:{
            offsetData = section->parRef->fileOffset + section->parOffset;
            dexCacheFile.pwrite(&section->fileOffset, 4, offsetData);
            break;
        }
        case typeCodeItem:{
            ClassDataSection* parent=(ClassDataSection*)section->parRef;
            offsetData = parent->codeRefs[section->parIdx].offset;
            writeUnsignedLeb128(dexCacheFile.getCache(offsetData), section->fileOffset);
            break;
        }

        default:{
            offsetData = (u4) (section->parStart + section->parOffset);
            dexCacheFile.pwrite(&section->fileOffset, 4, offsetData);
            break;
        }
    }
}
static void putAnnoSetItem( std::vector<::DataSection *>& dataSection,DataSection* par, u4 parOffset,const art::DexFile::AnnotationSetItem* setItem, u1* begin) {
    using namespace art;
    DataSection* setItemSect =new DataSection;
    setItemSect->type=typeAnnotationSetItem;
    setItemSect->size= setItem->size_*4+4;
    setItemSect->src=setItem;
    setItemSect->parRef=par;
    setItemSect->parOffset=parOffset;
    dataSection.push_back(setItemSect);
    //LOGV("Copied AnnosetItem,annoItem count=%u",setItem->size_);
    for(u4 i=0;i<setItem->size_;++i){
        const DexFile::AnnotationItem* item = reinterpret_cast<const DexFile::AnnotationItem*>(begin + setItem->entries_[i]);
        //pre-verify
        int size;
        readUnsignedLeb128(item->annotation_,size);

        DataSection* section = new DataSection;
        section->type = typeAnnotationItem;
        section->src = item;
        section->size = 1U+ getEncodedAnnotationItemSize(item->annotation_);
        section->parOffset = (i + 1) << 2;
        section->parRef=setItemSect;
        dataSection.push_back(section);
    }
}
static inline int jniNameLen(const char* classDescriptor,
                             const char* methodName){
    return 4+strlen(classDescriptor)+strlen(methodName);
}
static void fixMethodCodeIfNeeded(JNIEnv *env, const art::DexFile *dexFile, int methodSize,
                                  const jclass &thizClass,
                                  std::vector<::DataSection *> &dataSection, const u1 *&ptr,
                                  ClassDataSection *classData,
                                  std::vector<const art::DexFile *> &dex_files) {
    //LOGV("Into Fix");

    if(methodSize<=0)
        return ;
    int preIdx=0,methodIdx,size;
     u1* begin= const_cast<u1*>(dexFile->begin_);
    for(int i=0; i < methodSize; ++i){
        
        methodIdx=readUnsignedLeb128(size,ptr);//method_idx_diff
        int accessFlag = readUnsignedLeb128(size, ptr);//access_flags
        int codeOff=readUnsignedLeb128(ptr,size);
        //LOGV("accessFlag=%d,codeOff=%d",accessFlag,codeOff);
        methodIdx+=preIdx;
        preIdx=methodIdx;
        int realFlag=accessFlag;
        if(codeOff!=0||!isDalvik()){//actually, we can't fix code off in dalvik environment,maybe we should keep exploring.
            art::DexFile::CodeItem* codeItem=codeOff==0? nullptr: reinterpret_cast<art::DexFile::CodeItem*>(begin + codeOff);
            const art::DexFile::MethodId& methodId=dexFile->method_ids_[methodIdx];
            if(isFixCode()&&thizClass!= nullptr){
                //TODO:For Total method replacement, there may should be a more complex treatment
                const char* methodName= dexFile->stringByIndex(methodId.name_idx_);
                std::string sig = std::move(getProtoSig(methodId.proto_idx_, dexFile));
                jmethodID  thisMethodId;
                if ((accessFlag & dalvik::ACC_STATIC) == 0)
                    thisMethodId = env->GetMethodID(thizClass, methodName, &sig[0]);
                else thisMethodId = env->GetStaticMethodID(thizClass, methodName, &sig[0]);

                if(env->ExceptionCheck()==JNI_TRUE){
                    //LOGW("Expected Method not Found in %s%s of class %s",methodName,sig,dexGlobal->curClass);
                    env->ExceptionClear();
                    goto PutCodeItem;
                }
                if(isDalvik()){
                    dalvik::Method* meth=(dalvik::Method*)thisMethodId;
                    realFlag=meth->accessFlags;
                    const u2* insns=meth->insns;
                    ///LOGV("Running in dalvik,insns=%p",insns);
                    if (insns != NULL && insns != codeItem->insns_) {
                        //no so reliable as it seems that the insns size are unknown.
                        dalvik::ClassObject *classObject = reinterpret_cast<dalvik::Method *>(thisMethodId)->clazz;
                        dalvik::DexFile *rDexFile = classObject->pDvmDex->pDexFile;
                        if (rDexFile->baseAddr == dexFile->begin_) {
                            //if (isBadWritePtr(codeItem->insns_)) {
                                auto fCodeItem = (art::DexFile::CodeItem *) (
                                        reinterpret_cast<const u1 *>(insns) -
                                        offsetof(art::DexFile::CodeItem, insns_));
                                if (fCodeItem != codeItem) {
                                    LOGV("Code Item fake=%p real=%p", fCodeItem, codeItem);
                                    codeItem->toString();
                                    fCodeItem->toString();
                                    codeItem = fCodeItem;
                                }
                            //}
                            /*else {
                                LOGV("Into Write Code Item");
                                memmove(codeItem->insns_, insns, codeItem->insns_size_in_code_units_ * 2U);
                            }*/
                        }

                    }
                } else{
                    u4 rCodeOff;//r=real or runtime
                    GET_ART_METHOD_MEMBER_VALUE(rCodeOff, dex_code_item_offset_, thisMethodId);
                    GET_ART_METHOD_MEMBER_VALUE(realFlag,access_flags_,thisMethodId);
                    if (rCodeOff != codeOff) {
                        LOGE("Start Judge Code Off DexFile");
                        
                        u8 declaring_class;
                        GET_ART_METHOD_MEMBER_VALUE(declaring_class, declaring_class_, thisMethodId)
                        art::DexFile *rDexFile = getRealDexFile((ArtClass*)declaring_class);//r=real or runtime
                        //if (rDexFile != dexFile) {
                        //TODO:And your own codes if you want to analyse the dexFile loaded by dynamic fix e.g. AndFix.default is
                        /*if(std::find(dex_files.begin(),dex_files.end(),rDexFile)!=dex_files.end()&&env->CallStaticBooleanMethod(dexGlobal.getToolsClass()
                                ,env->GetStaticMethodID(dexGlobal.getToolsClass()
                                        ,"isSystemClass","(Ljava/lang/Class;)Z"),thizClass)){
                            dex_files.push_back(rDexFile);
                        };*/
                        //}
                        if (rDexFile == dexFile) {
                            const char *className = dexFile->stringFromType(
                                    methodId.class_idx_);
                            LOGE("Mismatch codeOff class=%s method=%s old=%u,new=%u", className,
                                 methodName, codeOff,
                                 rCodeOff);
                            if (rCodeOff !=
                                0) {//LOGV("Running in art,instructed code off=%u",codeOff);
                                codeItem = reinterpret_cast<art::DexFile::CodeItem *>(begin +
                                                                                      rCodeOff);
                            }
                        }
                    }
                }
            }
            PutCodeItem:
            if(accessFlag!=realFlag){
                std::string err(formatMethod(methodId,dexFile));
                LOGE("Error Native Method With Wrong Code off,%s",&err[0]);
            }
            if(codeItem!= nullptr){
                //LOGV("Into Code Item");
                
                CodeItemSect* section=new CodeItemSect;
                section->parIdx=classData->codeRefSize;
                ULebRef& uLeb=classData->codeRefs[(classData->codeRefSize)++];
                uLeb.origSize= (u1) size;
                uLeb.offset= (u4) (ptr - (const u1*)classData->src);
                section->type= typeCodeItem;
                section->src=  codeItem;
                section->size= sizeof(art::DexFile::CodeItem) - sizeof(u2);
                section->parRef=classData;
                section->size+=codeItem->insns_size_in_code_units_*2U;//insns are two byte unit

                /*SigCatcher::catchSig(SIGSEGV, [err,dexFile]() -> bool {
                    LOGV("Error Method=%s",err.c_str());
                    return false;
                });*/
                if(putCodeItem(dataSection,codeItem,section,begin)){
                    CodeResolver *resolver = new CodeResolver(env, codeItem, &methodId,
                                                              (accessFlag & dalvik::ACC_STATIC) ==
                                                              0);
                    std::string proto=getProtoSig(methodId.proto_idx_,dexFile);

                    LOGV("method %s%s of class %s needs to be fixed ",
                         dexFile->stringByIndex(methodId.name_idx_),&proto[0],dexFile->stringFromType(methodId.class_idx_));
                    section->fileOffsetRef = &resolver->fileOffset;
                    resolver->pend();
                };
                
                //LOGV("Code Out");
            }
        }
        ptr+=size;//keep this here for ref use;
    }
}


static bool putCodeItem(std::vector<::DataSection *>& dataSection,
               art::DexFile::CodeItem* codeItem,CodeItemSect* section, u1* begin) {
    //LOGV("Into put code");
    int size;
    if(codeItem->tries_size_>0){
        //LOGV("Try in");
        //LOGV("Has try part,try size=%u",codeItem->tries_size_);
        if((codeItem->insns_size_in_code_units_&1) ==1){
            section->size+=2U;
        }
        section->size+= sizeof(art::DexFile::TryItem) * codeItem->tries_size_;
        const u1*ptr = ((u1*)codeItem) + section->size;
        int handlerListSize=readUnsignedLeb128(ptr, size);
        section->size+=size;
        ptr +=size;
        for(int k=0;k<handlerListSize;++k){
            int handlerSize=readSignedLeb128(ptr, size);
            section->size+=size;
            ptr +=size;
            bool hasCatchAll=handlerSize<=0;
            handlerSize=handlerSize>0?handlerSize:-handlerSize;
            //LOGV("Catches size=%d",handlerSize);
            for(;handlerSize>0;--handlerSize){
                readUnsignedLeb128(size, ptr);
                section->size+=size;
                readUnsignedLeb128(size, ptr);
                section->size+=size;
            }
            if(hasCatchAll) {
                readUnsignedLeb128(size, ptr);
                section->size+=size;
            }
        }
        //LOGV("Try out");
       //LOGV("Try part size counted,now size=%d",section->size);
    }

    dataSection.push_back((DataSection*)section);
    u4 debugOff=codeItem->debug_info_off_;
    if(debugOff!=0){
        //LOGV("Debug in");
        const u1* ptr= begin+debugOff;
        if (isBadPtr(ptr)) {
            section->isValidDebugOff = false;
        } else {
            DataSection *debugSect = new DataSection;
            debugSect->src = ptr;
            debugSect->type = typeDebugInfoItem;
            debugSect->parRef = section;
            debugSect->parOffset = offsetof(art::DexFile::CodeItem, debug_info_off_);
            readUnsignedLeb128(size, ptr);//line_start
            int paraSize = readUnsignedLeb128(size, ptr);//parameters_size
            for (int i = 0; i < paraSize; ++i) {
                readUnsignedLeb128(size, ptr);
            }
            skimDebugCode(&ptr);
            debugSect->size = (u4) (ptr - begin - debugOff);
            dataSection.push_back(debugSect);
        }

        //LOGV("Debug Out");
    }
    return fixOpCodeOrNot(codeItem->insns_, codeItem->insns_size_in_code_units_);
}

bool fixOpCodeOrNot(u2 *insns, u4 insns_size) {
    u4 i;
    for (i = 0; i < insns_size;) {
        u1 opCode = u1((insns[i]) & (u2) 0xff);
        if (!isDalvik()) {
           if(opCode==0x73||(opCode>=0xe3&&opCode<=0xf9&&opCode!=0xf4)){
               return true;
           }
        } else if (opCode >= 0xe3 && opCode < 0xff) {
            return true;
        }


        if ((opCode > 0x3e && opCode <= 0x43) || (opCode >= 0x79 && opCode <= 0x7a) ||
            (opCode >= 0xef && opCode <= 0xff)/*unused code*/)
            LOGW("Unused opCode=%x", opCode);//in case these codes are used in future.
        if (opCode == 0x18/*51L*/) {
            i += 5;
            continue;
        }
        if (opCode == 0x0e || (opCode >= 0x3e && opCode <= 0x43) || opCode == 0x73
            || (opCode >= 0x79 && opCode <= 0x7a) || (opCode >= 0xe3 && opCode <= 0xff)//10x
            || opCode == 0x1 || opCode == 0x4 || opCode == 0x7 || opCode == 0x21
            || (opCode >= 0xb0 && opCode <= 0xcf) || (opCode >= 0x7b && opCode <= 0x8f)//12x
            || (opCode >= 0x0a && opCode <= 0x12) || opCode == 0x1d || opCode == 0x1e
            || opCode == 0x27/*11x &11n*/|| opCode == 0x28) {//10t
            ++i;
            continue;
        }
        if (opCode == 0x29/*20t*/|| opCode == 0x2 || opCode == 0x13 || opCode == 0x15
            || opCode == 0x16 || opCode == 0x19 || opCode == 0x1a || opCode == 0x1c
            || opCode == 0x1f || opCode == 0x20 || opCode == 0x22 || opCode == 0x23
            || (opCode >= 0x2d && opCode <= 0x3d) || (opCode >= 0x44 && opCode <= 0x6d)
            || opCode == 0x5 || opCode == 0x8 || (opCode >= 0xd0 && opCode <= 0xe2) ||
            (opCode >= 0x90 && opCode <= 0xaf)) {
            i += 2;
            continue;
        }
        if ((opCode >= 0x2a && opCode <= 0x2c) || opCode == 0x3 || opCode == 0x9 || opCode == 0x6
            || opCode == 0x14 || opCode == 0x17 || opCode == 0x1b || (opCode >= 0x24
                                                                      && opCode <= 0x26) ||
            (opCode >= 0x6e && opCode <= 0x78/*ignore 0x73 as it its cast*/)
                ) {
            i += 3;
            continue;
        }
        if (opCode == 0) {//nop or pseudo-code
            u1 tag = u1(insns[i] >> 8);
            switch (tag) {
                case PACKED_SWITCH: {
                    u2 size = insns[i + 1];
                    i += ((size * 2) + 4);
                    break;
                }
                case SPARSE_SWITCH: {
                    u2 size = insns[i + 1];
                    i += ((size * 4) + 2);
                    break;
                }
                case FILL_ARRAY_DATA: {
                    u2 width = insns[i + 1];
                    u4 size = *reinterpret_cast<u4 *>(&insns[i + 2]);
                    u4 tableSize = (size * width + 1) / 2 +
                                   4;//plus 1 in case width is odd,for even the plus is ignored
                    //required four byte alignment,but necessary?
                    /* if((tableSize&1)==1){
                         ++tableSize;
                     }*/
                    i += tableSize;
                    break;
                }
                default: {
                    if (tag != 0)
                        LOGV("Unrecognized 0 tag %x, skip it", tag);
                    ++i;
                    break;
                }

            }
        }
    }
    if (i != insns_size) {
        LOGE("Pos add wrong, check failed ");
    }
    return false;
}
static int skimDebugCode(const u1** pStream){
    using namespace art;
    const u1* ptr=*pStream;
    int size;
    Restart:
    switch(*ptr++){
        case DexFile::DBG_END_SEQUENCE:
            break;
        case DexFile::DBG_SET_PROLOGUE_END:
        case DexFile::DBG_SET_EPILOGUE_BEGIN:
            goto Restart;
        case DexFile::DBG_ADVANCE_PC:
        case DexFile::DBG_END_LOCAL:
        case DexFile::DBG_RESTART_LOCAL:
        case DexFile::DBG_SET_FILE:
            readUnsignedLeb128(size,ptr);
            goto Restart;
        case DexFile::DBG_ADVANCE_LINE:
            readSignedLeb128(ptr,size);
            ptr+=size;
            goto Restart;
        case DexFile::DBG_START_LOCAL_EXTENDED:
            readUnsignedLeb128(size,ptr);
        case DexFile::DBG_START_LOCAL:
            readUnsignedLeb128(size,ptr);
            readUnsignedLeb128(size,ptr);
            readUnsignedLeb128(size,ptr);
            goto Restart;
        default:
            goto Restart;
    }
    size=int(ptr-*pStream);
    *pStream=ptr;
    //LOGV("Debug code size=%d",size);
    return size;
}

static void putTypeList(std::vector<:: DataSection * >& dataSection, const long offset,u4 paraStart,u4 parOffset,
                        u2 type,
                         u1* begin) {
    //LOGV("Start put type list");
	using namespace art;
	const DexFile::TypeList* list = reinterpret_cast<const DexFile::TypeList*>(offset + begin) ;
	DataSection* section = new DataSection();
	section->type = (DexDataType) type;
	section->size =(int) list->GetListSize(list->Size());
	section->parOffset = parOffset;
    section->parStart=paraStart;
	section->src =  list;
	dataSection.push_back(section);
}


static int getEncodedValueSize(const Encoded_Value *encodedValue) {
	u1 args_type = encodedValue->valueArgsAndType;
	u1 type = args_type & (u1)0x1f;
	u1 args = (args_type & (u1)0xe0)>>5;//Must move!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	switch (type) {
	    case kBoolean:
	    case kNull: return 1U;
	    case kArray: return 1U+getEncodedArraySize(encodedValue->value);
	    case kAnnotation: return 1U+getEncodedAnnotationItemSize(encodedValue->value);
	    default:return 2U + args;//args now=size-1 total=size+1,so return args+2;
	}

}
static int getEncodedArraySize(const u1* encodedArray) {
	int size;
	int total=readUnsignedLeb128(encodedArray,/*by ref*/ size);
	const u1* ptr =encodedArray + size;
	for (int i = 0;i < total;++i) {
		size = getEncodedValueSize(reinterpret_cast<const Encoded_Value *>(ptr));
		ptr += size;
	}
	return int(ptr - encodedArray);
}
static int getEncodedAnnotationItemSize(const u1* encodedItem) {
	int size;
    const u1* ptr = encodedItem ;
    int type=readUnsignedLeb128(size,ptr);//read type_idx
    try {
        dexGlobal.dexFile->stringFromType(type);
    }catch (std::out_of_range&e){
        system("logcat -c");
        LOGE("Invalid anno type");
        throw e;
    }

    int annosize = readUnsignedLeb128(size, ptr);//read size
	for (int i = 0;i < annosize;++i) {
        readUnsignedLeb128(size, ptr);//name_idx element name
		size = getEncodedValueSize(reinterpret_cast<const Encoded_Value *>(ptr));
		ptr += size;
	}
	return int(ptr - encodedItem);
}

static void setDexName(int count, char* dexName) {
    int i = 0;
    for (int num;count > 0;count /= 10) {
        num = count % 10;
        dexName[i++] = (char)num + '0';
    }
    dexName[i] = '\0';
    dexName=my_strrev(dexName );
    strcat(&dexName[i], ".dex");
}
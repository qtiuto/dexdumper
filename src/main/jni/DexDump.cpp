//
// Created by asus on 2016/7/18.
//

#include "DexDump.h"
#include <algorithm>

const char fill[] ="\0\0\0\0\0\0\0\0";
DexGlobal dexGlobal;
static bool isLog;
JavaVM *javaVM;
struct DataSection;
struct LinkData;
struct Encoded_Value;
struct HeadSection;


enum DexDataType;
#define SIG_METHOD_NOT_FOUND 34

void handleJniCrash(JNIEnv *env, int sig, void *extra){
    //jclass dexDumpException=env->FindClass("com/oslorde/extra/DexDumper$DexDumpException");
    if(sig==SIG_METHOD_NOT_FOUND){
        jthrowable throwable=env->ExceptionOccurred();
        env->Throw(throwable);
    }
}
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved){
    JNIEnv* env;
    if(vm->GetEnv((void**)&env,JNI_VERSION_1_4)!=JNI_OK){
        return JNI_ERR;
    }
    javaVM=vm;
    LOGV("Vm putted");
    return JNI_VERSION_1_4;
}
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_setMode(JNIEnv *env, jclass thisClass, jint mode){
    dexGlobal.dumpMode = (u1) mode;
    u1 size=(u1)((u4)mode>>24);
    if(size!=0){
        dexGlobal.poolSize=size;
    }
}
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_dumpDexV14(JNIEnv *env,jclass thisClass,jlong cookie,jstring baseOutDir){
    dalvik::DexOrJar* pDexOrJar = (dalvik::DexOrJar*) cookie;
    dalvik::DvmDex* pDvmDex;
    if(pDexOrJar->isDex){
        pDvmDex=pDexOrJar->pRawDexFile->pDvmDex;
    } else pDvmDex=pDexOrJar->pJarFile->pDvmDex;
    dalvik::DexFile* dex_file=pDvmDex->pDexFile;
    dalvik::DexFile* dexFile=new dalvik::DexFile;
    memcpy(dex_file,dexFile, sizeof(dalvik::DexFile));
    const dalvik::DexHeader* header1= pDvmDex->pHeader;
    const dalvik::DexHeader* header2=dexFile->pHeader;
    bool isF=true;
    if(header1!=header2){
        LOGW("Header unequal,check it");
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
    dexGlobal.sdkOpt=DALVIK;
    dumpDex(env,dex_files,outDir);
    env->ReleaseStringUTFChars(baseOutDir, outDir);
    delete dexFile;
}
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_dumpDexV19ForArt(JNIEnv *env,jclass thisClass,jlong cookie,jstring baseOutDir){
    using namespace art;
    dexGlobal.sdkOpt=ART_KITKAT;
    const DexFile* dexFile= reinterpret_cast<DexFile*>(cookie);
    std::vector<const art::DexFile*> dex_files;
    dex_files.reserve(1);
    dex_files.push_back(dexFile);
    jboolean isCopy ;
    const char* outDir= env->GetStringUTFChars(baseOutDir, &isCopy );
    dumpDex(env,dex_files,outDir);
    env->ReleaseStringUTFChars(baseOutDir, outDir);
}
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_dumpDexV21(JNIEnv *env, jclass thisClass
        ,jlong dex_file_address, jstring baseOutDir,jboolean isMr1){
    dexGlobal.sdkOpt=isMr1?ART_LOLLIPOP_MR1:ART_LOLLIPOP;
    std::vector<const art::DexFile*>* dex_files = reinterpret_cast<std::vector<const art::DexFile*>*>(
		static_cast<uintptr_t>(dex_file_address));
	jboolean isCopy ;
	const char* outDir=env->GetStringUTFChars(baseOutDir, &isCopy );
	dumpDex(env,dex_files[0], outDir);
	env->ReleaseStringUTFChars(baseOutDir, outDir);
}
JNIEXPORT  void JNICALL Java_com_oslorde_extra_DexDumper_dumpDexV23(JNIEnv *env,jclass thisClas
        ,jlongArray dex_files_address,jstring baseOutDir, jboolean isNougat){
    dexGlobal.sdkOpt=isNougat?ART_NOUGAT:ART_MARSHMALLOW;
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
	env->ReleaseStringUTFChars(baseOutDir, outDir);
}
static void dumpDex(JNIEnv* env,std::vector<const art::DexFile*>& dex_files,const char* outDir){
    using namespace art;
	int dirLen = (int) strlen(outDir);
	char dexName[dirLen +23];

    dexGlobal.dexFileName=dexName;

    memset(dexName, '\0', (size_t) (dirLen + 23));
	memcpy(dexName, outDir, (size_t) dirLen);
    if(outDir[dirLen-1]!='/'){
       dexName[dirLen++]='/';
    }
    int count=0;
    struct dirent *dp;
    DIR *dirp = opendir(dexName); //打开目录指针
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
		setDexName(count, dexName+dirLen);
        LOGV("Dex out %s",dexName);
        int fd=open(dexName,O_RDWR|O_CREAT,00700);
        if(fd==-1){
            LOGE("Open File: %s Failed",dexName);
            continue;
        } else{
            struct stat buf;
            if(fstat(fd,&buf)==-1){
                perror("Oslorde_dexdumper:Can not read or write the dex file");
                continue;
            }
        }
        dexGlobal.dexFile=dex;
        LOGV("Global dexFile=%p",dex);
        if(dexGlobal.pool!= nullptr)
            dexGlobal.pool->reOpen();
		std::vector<::DataSection*> dataSection;
        HeadSection* heads=new HeadSection[7];
        LinkData* linkData= nullptr;
        u1* begin = const_cast<u1*>(dex->begin_);
        LOGV("start address,%p",begin);
		DexFile::Header header =*dex->header_ ;
		if (memcmp(header.magic_, "dex\n",4) != 0 &&judgeVersion(&header.magic_[4])) {
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
		heads[0].ptr= (u1 *)& header;
        heads[0].num = 1;
        heads[0].size= sizeof(DexFile::Header);
        header.header_size_=sizeof(DexFile::Header);

        LOGV("Go to MapList");
        char buf[sizeof(DexFile::MapItem)*18/*the max num of size*/ + 4U];
        DexFile::MapList* mapList=new(buf) DexFile::MapList;
        DataSection* mapSection=new DataSection;
        mapSection->type=typeMapList;
        mapSection->src=mapList;//Don't use ptr directly add or substract unless it's u1* or char*
        mapSection->size=0;// to be fix in the nature
        dataSection.push_back(mapSection);//mapof will be updated directly on header;

        LOGV("Go to String ids");
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
        LOGV("Go to Type ids");
        header.type_ids_off_=header.string_ids_off_+heads[1].size;
		const DexFile::TypeId* typeIds = dex->type_ids_;
		idNum = header.type_ids_size_;
        heads[2].ptr= (u1 *) typeIds;
        heads[2].num = idNum;
        heads[2].size=idNum*(u4) sizeof(DexFile::TypeId);

        LOGV("Go to Proto ids,size=%d",idNum);
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

        LOGV("Go to Field ids");
        header.field_ids_off_=header.proto_ids_off_+heads[3].size;
		const DexFile::FieldId* fieldIds = dex->field_ids_;
		idNum = header.field_ids_size_;
        heads[4].ptr= (u1 *) fieldIds;
        heads[4].num = idNum;
        heads[4].size=idNum* (u4)sizeof(DexFile::FieldId);

        LOGV("Go to Method ids");
        header.method_ids_off_=header.field_ids_off_+heads[4].size;
		const DexFile::MethodId* methodIds = dex->method_ids_;
		idNum = header.method_ids_size_;
        heads[5].ptr= (u1 *) methodIds;
        heads[5].num =  idNum;
        heads[5].size=idNum*(u4) sizeof(DexFile::MethodId);

        LOGV("Go to classDef");
        header.class_defs_off_=header.method_ids_off_+heads[5].size;
		const DexFile::ClassDef* classDefs = dex->class_defs_;
        idNum = header.class_defs_size_;
        heads[6].num = idNum;
        heads[6].size= idNum * (u4) sizeof(DexFile::ClassDef);
        heads[6].ptr= (u1 *) classDefs;

        dexGlobal.setToolsClass(env);

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
        delete[] heads;
        //LOGV("Start resolve clas def");
		for (int i = 0; i < idNum; ++i) {
			const DexFile::ClassDef&clsDefItem = *(classDefs + i);
            const DexFile::TypeId& clsType=typeIds[clsDefItem.class_idx_];
            const char* clsChars=getStringFromStringId(stringIds[clsType.descriptor_idx_],begin);
            //LOGV("Start put cls data,cls name:%s,classIdx=%u",clsChars,clsDefItem.class_idx_);
            //isLog=strcmp(clsChars,"Landroid/media/MediaMetadataRetriever;")==0;
            dexGlobal.curClass=clsChars;
            jclass thizClass= nullptr;
            if(isFixCode()){
                char *fixedClssName = toJavaClassName(clsChars);
                jstring clsName=env->NewStringUTF(fixedClssName);//skip primitive class
                thizClass = (jclass) env->CallStaticObjectMethod(toolsClass, findMethod, clsName);
                if(thizClass==NULL){
                    thizClass= nullptr;
                    LOGW("Class %s can't be init in jni environment,\n"
                                 "may be they are rejected for referring "
                                 "to inaccessible class,e.g. xposedbridge.",fixedClssName);
                    if(isFixCode())LOGW("So,dynamic code fix is disabled for this class");
                }
                env->DeleteLocalRef(clsName);
                delete[] fixedClssName;
            }
			if (clsDefItem.interfaces_off_ != 0){
                //LOGV("Interfaces off=%u",clsDefItem.interfaces_off_);
                putTypeList(dataSection, clsDefItem.interfaces_off_,i* (u4)sizeof(DexFile::ClassDef)
                           +header.class_defs_off_, offsetof(DexFile::ClassDef
                ,interfaces_off_), typeInterface, begin);
            }
			if (clsDefItem.annotations_off_ != 0) {
                //LOGV("AnnoDir Offset=%u",clsDefItem.annotations_off_);
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

                //LOGV("Copied AnnoDir, size=%d,offset=%ld",section->size,section->offset);
				dataSection.push_back(section);
                //LOGV("directoryBuf ptr=%p,directory=%p",(void*)directoryBuf,directory);
                u1*ptrCur = reinterpret_cast<u1*>(directory+ 1U);//as directory is of that type so just offset 1
                u1* ptrStart= (u1 *) directory;
                //LOGV("ptrCur=%p,sizeof=%d",(void*)ptrCur,sizeof(DexFile::AnnotationsDirectoryItem));
                u4 classAnnoOffset=directory->class_annotations_off_;
                //LOGV("Get classAnnoOffset over,offset=%u",classAnnoOffset);
                if(classAnnoOffset!=0){
                    DexFile::AnnotationSetItem* setItem= reinterpret_cast<DexFile::AnnotationSetItem*>(begin+classAnnoOffset);
                    putAnnoSetItem(dataSection, section, 0, setItem, begin);
                }
				if (directory->fields_size_>0) {
                    //LOGV("Field annotations not null,size=%u,ptr=%p", directory->fields_size_, ptrCur);

					for (int j = 0;j < directory->fields_size_;++j) {
						DexFile::FieldAnnotationsItem* item = reinterpret_cast<DexFile::FieldAnnotationsItem*>(ptrCur);
                        DexFile::AnnotationSetItem* setItem= reinterpret_cast<DexFile::AnnotationSetItem*>(begin+item->annotations_off_);
                        putAnnoSetItem(dataSection, section, (u4) (ptrCur - ptrStart) +offsetof(
                                DexFile::FieldAnnotationsItem,annotations_off_), setItem, begin);
						ptrCur += sizeof(DexFile::FieldAnnotationsItem);
					}

				}
				if (directory->methods_size_>0) {
                    //LOGV("Method annotations not null,size=%u,offset=%p", directory->methods_size_, ptrCur-begin);
					for (int j = 0;j < directory->methods_size_;++j) {
						DexFile::MethodAnnotationsItem* item = reinterpret_cast<DexFile::MethodAnnotationsItem*>(ptrCur);
                        //LOGV("Method annotationItem,offset=%u",item->annotations_off_);
                        DexFile::AnnotationSetItem* setItem= reinterpret_cast<DexFile::AnnotationSetItem*>(begin+item->annotations_off_) ;
                       // LOGV("Method annotationItem,offset=%u,counts=%u",item->annotations_off_,setItem->size_);
                        putAnnoSetItem(dataSection, section, (u4) (ptrCur - ptrStart)+offsetof(
                                DexFile::MethodAnnotationsItem,annotations_off_), setItem, begin);
						ptrCur += sizeof(DexFile::MethodAnnotationsItem);
					}
				}
                if(directory->parameters_size_>0){
                    //LOGV("Parameter annotations not null,size=%u,ptr=%p", directory->parameters_size_, ptrCur);
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
                            DexFile::AnnotationSetItem* setItem= reinterpret_cast<DexFile::AnnotationSetItem*>(begin+setRefItem->annotations_off_);
                            putAnnoSetItem(dataSection,sect,(k+1)<<2,setItem,begin);
                        }
                        ptrCur += sizeof(DexFile::ParameterAnnotationsItem);
                    }
                }
			}

            if(clsDefItem.class_data_off_ != 0){
               // LOGV("Class data off=%u",clsDefItem.class_data_off_);
                //usleep(1000);
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
                char*dataBuf =new char[sizeof(ClassDataSection) + sizeof(ULebRef) * (virtualMethodSize + directMethodSize)];
                ClassDataSection* section=new(dataBuf)ClassDataSection;
                section->size=u4(ptr-beginPtr);
                //LOGV("class data size gotten=%d",section->size);
                section->src=beginPtr;

                section->type=typeClassDataItem;
                section->parOffset=offsetof(DexFile::ClassDef,class_data_off_);
                section->parStart=header.class_defs_off_+i*sizeof(DexFile::ClassDef);
                dataSection.push_back(section);

                ptr=beginPtr+methodBeginSize;

                //LOGV("Start Fix Direct Method,ptr=%p",ptr);

                fixMethodCodeIfNeeded(env, dex, directMethodSize, thizClass,
                                      dataSection, ptr,section , clsDefItem.class_idx_);

                //LOGV("Start Fix Virtul Method,ptr=%p",ptr);
                fixMethodCodeIfNeeded(env, dex, virtualMethodSize, thizClass,
                                      dataSection, ptr,section , clsDefItem.class_idx_);
            }
            if(clsDefItem.static_values_off_ != 0){
                //LOGV("Static values off,%u",clsDefItem.static_values_off_);
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
        //lseek(fd,0,SEEK_SET);
        pwrite(fd,&header, sizeof(DexFile::Header),0);
        LOGV("Start writing,data_off=%u,data_size=%u,file size=%u",header.data_off_,header.data_size_,fileSize);
        truncate(dexName,fileSize);

		lseek(fd,header.data_off_,SEEK_SET);
        LOGV("Start write data Section pos=%ld",lseek(fd,0,SEEK_CUR));
		for (DataSection* section : dataSection) {
            writeDataSection(fd,section);
		}
        if(lseek(fd,0,SEEK_CUR)-header.data_off_!=header.data_size_){
            LOGW("Write dex file wrong,some elements are "
                         "forgotten to be emplaced,now pos=%ld",lseek(fd,0,SEEK_CUR));
        }
        if(linkData!= nullptr){
            LOGV("Start writing link data");
            write(fd,(const char*)linkData->data, linkData->size);
            delete linkData;
        }
        LOGV("Update ref");
        //usleep(2000);
        for(DataSection* section:dataSection){
            updateRef(fd,section);
        }
        LOGV("delete data");
        for(DataSection* section:dataSection){
            if(section->type!=typeClassDataItem)
                delete section;
            else delete [] (char*)(section);
        }
        LOGV("Execute pending");
        //usleep(2000);
        if(dexGlobal.pool!= nullptr){
            dexGlobal.pool->executeAllPendingTasks();
            dexGlobal.pool->waitForFinish();
        }
        LOGW("Wait over!Start writing new header");
        jstring dexPath=env->NewStringUTF(dexName);
        jmethodID hashMid=env->GetStaticMethodID(toolsClass, "getDexSHA1Hash", "(Ljava/lang/String;)[B");
        jbyteArray fileContent= (jbyteArray) env->CallStaticObjectMethod(toolsClass, hashMid, dexPath);

        env->DeleteLocalRef(dexPath);
        jbyte * readBuf= env->GetByteArrayElements(fileContent, 0);
        //LOGW("SHAl Count");
        DexFile::Header* fileHeader= reinterpret_cast<DexFile::Header*>(readBuf);
        unsigned int adler32(char *data, size_t len);
        fileHeader->checksum_= adler32((char *) fileHeader->signature_, fileSize - 12);
        //fd=open(dexName,O_RDWR|O_CREAT);

        pwrite(fd,(const char*)fileHeader, sizeof(DexFile::Header),0);
        close(fd);
        env->ReleaseByteArrayElements(fileContent,readBuf,JNI_ABORT);
        env->DeleteLocalRef(fileContent);

        LOGV("New Header Written");
       // usleep(1000);
        LOGV("One dex over");
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
        //usleep(500);
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
    //usleep(50000);
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
                    //LOGV("nullptr found, keep first,type=%s",getDexDataTypeName(states[index].type));
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
                    //LOGV("nullptr found, keep first,type=%s",getDexDataTypeName(states[index].type));
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
                    //safe move;
                    //LOGV("ref=%p,curOffset=%u,curRefSize=%d",codeSect->ref,curOffset,curRefSize);
                    //LOGV("written size=%d",int(-p)) ;
                }else{
                    states[index].isFirst= true;
                    LOGV("nullptr found, keep first,type=%s",getDexDataTypeName(states[index].type));
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
                    LOGV("nullptr found, keep first,type=%s",getDexDataTypeName(states[index].type));
                }
                if((paddingRequired= (u1) (4 - section->size % 4)) != 4){
                    section->postPaddingSize=paddingRequired;
                    size+=paddingRequired;//4 byte alignment
                }
                size+=section->size;
                break;
            }
            case typeInterface:{
                //LOGW("Reached typeInterface,ref=%p",section->ref);
                changeItemState(states,index,section,size,data_off,section->type, true);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else{
                    states[index].isFirst= true;
                    //LOGV("nullptr found, keep first,type=%s",getDexDataTypeName(states[index].type));
                }
                if((paddingRequired= (u1) (4 - section->size % 4)) !=  4){
                    section->postPaddingSize=paddingRequired;
                    size+=paddingRequired;//4 byte alignment
                }
                size+=section->size;
                break;
            }
            case typeParameter:{
                //LOGW("Reached typeParameter");
                changeItemState(states,index,section,size,data_off,section->type, true);
                if(section->src!=nullptr)
                    section->fileOffset=data_off+size;
                else{
                    states[index].isFirst= true;
                    //LOGV("nullptr found, keep first,type=%s",getDexDataTypeName(states[index].type));
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
                    //LOGV("Class data off=%u",section->fileOffset);
                    size+=section->size;
                    for(int i=0;i<((ClassDataSection*)section)->codeRefSize;++i){
                        ULebRef& ref=((ClassDataSection*)section)->codeRefs[i];
                        size=size-ref.origSize+ref.nowSize;
                    }
                    //LOGV("classData%d Size=%u",num++,size-section->fileOffset+data_off);
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
    for(DataSection * sect:dataSection){
         curType=sect->type;
        if(lastType!=curType) {
            preType=lastType;
            //LOGV("curType=%s,preType=%s",getDexDataTypeName((u2)curType),getDexDataTypeName((u2)preType));
        }
        if(curType-preType>=2){
            while((++preType)<curType){
                DataSection* empty=new DataSection;
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
static void writeDataSection(int fd,DataSection* section){

	write(fd,fill, section->prePaddingSize);
    if(section->type!=typeClassDataItem){
        if(lseek(fd,0,SEEK_CUR)!=section->fileOffset&&section->fileOffset!= 0){
            LOGW("Offset off type %s mismatch",getDexDataTypeName(section->type));
        }
        write(fd,section->src,section->size);
    }
    else{
        ClassDataSection* classData= static_cast<ClassDataSection*>(section);
        //long off=lseek(fd,0,SEEK_CUR);
        u1 * ptr= (u1 *) section->src;int offset =0;
        for(int i=0;i<classData->codeRefSize;++i){
            ULebRef& ref=classData->codeRefs[i];
            write(fd, ptr + offset, ref.offset - offset);
            offset = ref.offset + ref.origSize;
            ref.offset= (u4) (lseek(fd, ref.nowSize, SEEK_CUR) - ref.nowSize);
        }
        write(fd, ptr + offset,classData->size-offset);
        //LOGV("Class Data%d, wrriten size=%ld",num++,lseek(fd,0,SEEK_CUR)-off);
    }
	write(fd,fill, section->postPaddingSize);
}

static void updateRef(int fd,DataSection* section){
    if(section->fileOffset== 0) return;
    u4 offset;
    switch (section->type){
        case typeMapList:
            return;//no need
        case typeAnnotationSetItem:
        case typeAnnotationSetRefList:
        case typeAnnotationItem:
        case typeDebugInfoItem:{
            offset=section->parRef->fileOffset+section->parOffset;
            //lseek(fd,offset,SEEK_SET);
            pwrite(fd,&section->fileOffset,4,offset);
            break;
        }
        case typeCodeItem:{
            ClassDataSection* parent=(ClassDataSection*)section->parRef;
            offset=parent->codeRefs[section->parIdx].offset;
            //lseek(fd,offset,SEEK_SET);
            writeUnsignedLeb128ToFile(fd,section->fileOffset,offset);
            break;
        }

        default:{
            offset= (u4) (section->parStart + section->parOffset);
            //lseek(fd,offset,SEEK_SET);
            pwrite(fd,&section->fileOffset,4,offset);
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
        DataSection* section = new DataSection;
        section->type = typeAnnotationItem;
        section->src = item;
        section->size = 1U+ getEncodedAnnotationItemSize(item->annotation_);
        section->parOffset =(i+1)<<2 ;
        section->parRef=setItemSect;
        dataSection.push_back(section);
    }
}
static void fixMethodCodeIfNeeded(JNIEnv *env,const art::DexFile* dexFile,int methodSize,
                                   const jclass &thizClass, std::vector<::DataSection *> &dataSection,
                                   const u1 *&ptr/*must keep this ref to be follow change*/,ClassDataSection* classData, u4 clsTypeIdx) {

    if(methodSize<=0)
        return ;
    int preIdx=0,methodIdx,size;
     u1* begin= const_cast<u1*>(dexFile->begin_);
    for(int i=0; i < methodSize; ++i){
        methodIdx=readUnsignedLeb128(size,ptr);//method_idx_diff
        int acessFlag=readUnsignedLeb128(size,ptr);//access_flags
        int codeOff=readUnsignedLeb128(ptr,size);
        //LOGV("accessFlag=%d,codeOff=%d",acessFlag,codeOff);
        methodIdx+=preIdx;
        preIdx=methodIdx;
        if(codeOff!=0||!isDalvik()){//actually, we can't fix code off in dalvik environment,maybe we should keep exploring.
            art::DexFile::CodeItem* codeItem=codeOff==0? nullptr: reinterpret_cast<art::DexFile::CodeItem*>(begin + codeOff);
            const art::DexFile::MethodId& methodId=dexFile->method_ids_[methodIdx];
            if(isFixCode()&&thizClass!= nullptr){
                //LOGV("Fix Code Required,Start Fix,methodIdx=%d",methodIdx);
                const char* methodName= getStringFromStringId(dexFile->string_ids_[methodId.name_idx_], begin);
                char* sig=getProtoSig(dexFile->proto_ids_[methodId.proto_idx_], dexFile->type_ids_, dexFile->string_ids_, begin);
                if(isLog)LOGV("Method =%s%s",methodName,sig);
                jmethodID  thisMethodId;
                if((acessFlag&dalvik::ACC_STATIC)==0)
                    thisMethodId = env->GetMethodID(thizClass, methodName, sig);
                else thisMethodId = env->GetStaticMethodID(thizClass, methodName, sig);

                if(env->ExceptionCheck()==JNI_TRUE){
                    //LOGW("Expected Method not Found in %s%s of class %s",methodName,sig,dexGlobal->curClass);
                    env->ExceptionClear();
                    delete[] sig;
                    goto PutCodeItem;
                }
                delete[] sig;
                if(isDalvik()){
                    dalvik::Method* meth=(dalvik::Method*)thisMethodId;
                    const u2* insns=meth->insns;
                    //LOGV("Running in dalvik,insns=%p",insns);
                    if(insns!= NULL){
                        //no so reliable as it seems that the insns size are unknown.
                        memmove(codeItem->insns_,insns,codeItem->insns_size_in_code_units_*2U);
                    }
                } else{
                    u4 codeff;
                    GET_ART_METHOD_MEMBER_VALUE(codeff,dex_code_item_offset_,thisMethodId);
                    if(codeff!=codeOff){
                        LOGW("Mismatch codeoff f=%u,s=%u",codeOff,codeff);
                    }
                    //LOGV("Running in art,instructed code off=%u",codeOff);
                    if(codeOff!=0) {
                        codeItem= reinterpret_cast<art::DexFile::CodeItem*>(begin + codeOff);
                    } //else codeItem= nullptr;
                }
            }
            PutCodeItem:
            if(codeItem!= nullptr){
                if(isLog)LOGV("Into Code Item");
                CodeItemSect* section=new CodeItemSect;
                section->parIdx=classData->codeRefSize;
                ULebRef& uLeb=classData->codeRefs[(classData->codeRefSize)++];
                uLeb.origSize= (u1) size;
                uLeb.offset= (u4) (ptr - (const u1*)classData->src);
                section->type= typeCodeItem;
                section->src=  codeItem;
                section->size= sizeof(art::DexFile::CodeItem) - sizeof(u2);
                section->parRef=classData;
                if(isLog){
                    LOGV("RagSize=%u,tries=%u,debugOff=%u",codeItem->registers_size_,codeItem->tries_size_,codeItem->debug_info_off_);
                }
                if(isLog)LOGV("Now Size without insns=%d,insns size=%u",section->size,codeItem->insns_size_in_code_units_);
                section->size+=codeItem->insns_size_in_code_units_*2U;//insns are two byte unit
                //usleep(2000);
                if(putCodeItem(dataSection,codeItem,section,begin)){
                    //LOGV("Into put resolver");
                    //if(strncmp(dexGlobal.curClass,"Lcom/a/a/a",10)==0){
                        CodeResolver* resolver=new CodeResolver(env,codeItem, clsTypeIdx, &methodId, (acessFlag & dalvik::ACC_STATIC) == 0);
                        section->fileOffsetRef=&resolver->fileOffset;
                        resolver->pend();
                    //}

                };
            }
        }
        ptr+=size;//keep this here for ref use;
    }
}

static bool putCodeItem(std::vector<::DataSection *>& dataSection,
               art::DexFile::CodeItem* codeItem,CodeItemSect* section, u1* begin) {
    if(isLog)LOGV("Start put code item,offset=%lu",(unsigned long)codeItem-(unsigned long)begin);
    //usleep(2000);
    int size;
    if(codeItem->tries_size_>0){
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
                //LOGV("Has Catch All,add its size=%d",size);
            }
        }
       //LOGV("Try part size counted,now size=%d",section->size);
    }
    //LOGV("Code Item Copied,ptr=%p,offset=%ld",codeBuf,section->offset);

    dataSection.push_back(section);
    u4 debugOff=codeItem->debug_info_off_;
    if(debugOff!=0){
        //LOGV("Into put debug item");
        DataSection* debugSect=new DataSection;
        const u1* ptr= begin+debugOff;
        debugSect->src=ptr;
        debugSect->type=typeDebugInfoItem;
        debugSect->parRef=section;
        debugSect->parOffset=offsetof(art::DexFile::CodeItem,debug_info_off_);
        readUnsignedLeb128(size,ptr);//line_start
        int paraSize=readUnsignedLeb128(size,ptr);//parameters_size
        for(int i=0;i<paraSize;++i){
            readUnsignedLeb128(size,ptr);
        }
        skimDebugCode(&ptr);
        debugSect->size= (u4) (ptr - begin - debugOff);
        dataSection.push_back(debugSect);
    }
    if(!isDalvik()){
        return fixOpCodeOrNot(codeItem);
    }
    return false;
}

static bool fixOpCodeOrNot(art::DexFile::CodeItem *codeItem) {
    //LOGV("Into fix opcode");
    int i;
    for(i=0;i<codeItem->insns_size_in_code_units_;) {
        u1 opCode = u1((codeItem->insns_[i]) & (u2) 0xff);
        switch (opCode) {
            case 0x73:
            case 0xe3:
            case 0xe4:
            case 0xe5:
            case 0xe6:
            case 0xe7:
            case 0xe8:
            case 0xe9:
            case 0xea:
            case 0xeb:
            case 0xec:
            case 0xed:
            case 0xee:
            case 0xef:
            case 0xf0:
            case 0xf1:
            case 0xf2:
            case 0xf3:
            case 0xf5:
            case 0xf6:
            case 0xf7:
            case 0xf8:
            case 0xf9:
            {
                return true;
            }
            default: {
                break;
            }
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
            u1 tag = u1(codeItem->insns_[i] >> 8);
            switch (tag) {
                case PACKED_SWITCH: {
                    u2 size = codeItem->insns_[i + 1];
                    i += ((size * 2) + 4);
                    break;
                }
                case SPARSE_SWITCH: {
                    u2 size = codeItem->insns_[i + 1];
                    i += ((size * 4) + 2);
                    break;
                }
                case FILL_ARRAY_DATA: {
                    u2 width = codeItem->insns_[i + 1];
                    u4 size = *reinterpret_cast<u4 *>(&codeItem->insns_[i + 2]);
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
    if(i!=codeItem->insns_size_in_code_units_){
        LOGE("Pos add wrong, check failed at class=%s",dexGlobal.curClass);
    }
    //LOGV("Fix OpCode Over");
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
	case kArray: return 1U+getEncodedArraySize(reinterpret_cast<const u1*>(encodedValue->value));
	case kAnnotation: return 1U+getEncodedAnnotationItemSize(reinterpret_cast<const u1*>(encodedValue->value));
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
	readUnsignedLeb128(encodedItem, size/*by ref*/);//read type_idx 	uleb128 type of the annotation. This must be a class (not array or primitive) type.
	const u1* ptr = encodedItem + size;
	int annosize = readUnsignedLeb128(size,ptr);//read size 	uleb128 	number of name-value mappings in this annotation
	for (int i = 0;i < annosize;++i) {
		readUnsignedLeb128(size,ptr);//name_idx element name, represented as an index into the string_ids section. The string must conform to the syntax for MemberName, defined above.
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
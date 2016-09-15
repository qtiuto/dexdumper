//
// Created by asus on 2016/8/16.
//
#include "CodeResolver.h"
JNINativeMethod getMethods[]={
        {"getFieldOffset","(Ljava/lang/reflect/Field;)I",(void*)getFieldOffset},
        {"getMethodVIdx","(Ljava/lang/reflect/Method;)I",(void*)getMethodVIdx}
};
extern JavaVM* javaVM;
static thread_local bool isLog;
static thread_local JNIEnv* env;
static class ref{
public:
    u4 strTypeIdx;
    u4 clsTypeIdx;
    const art::DexFile* curDexFile= nullptr;
} globalRef;
jint  getFieldOffset(JNIEnv *env,jclass thisClass,jobject field){
    jfieldID fieldID=env->FromReflectedField(field);
    if(isDalvik()){
        dalvik::InstField* instField= reinterpret_cast<dalvik::InstField*>(reinterpret_cast<u1*>(fieldID));
        return instField->byteOffset;
    }

    if(isKitkatArt()){
        ArtFieldKitkat* artFieldKitkat= reinterpret_cast<ArtFieldKitkat*>(reinterpret_cast<u1*>(fieldID));
        return artFieldKitkat->offset_;
    } else{
        ArtField* artField= reinterpret_cast<ArtField*>(reinterpret_cast<u1*>(fieldID));
        return artField->offset_;
    }// optimized field are u2 only
}
jint  getMethodVIdx(JNIEnv *env,jclass thisClass,jobject method){
    jmethodID methodID =env->FromReflectedMethod(method);
    if(isDalvik()){
        dalvik::Method* meth= reinterpret_cast<dalvik::Method*>(reinterpret_cast<u1*>(methodID));
        return meth->methodIndex;
    }
    u4 index;
    GET_ART_METHOD_MEMBER_VALUE(index,method_index_,methodID);
    return index;
}
void CodeResolver::threadInit() {
    javaVM->AttachCurrentThread(&env, nullptr);
    if(env->RegisterNatives(dexGlobal.getToolsClass(), getMethods, 2) < 0){
        javaVM->DetachCurrentThread();
        throw std::runtime_error("Register native method failed");
    }
    LOGV("Vm thread attached");
}
void CodeResolver::threadDestroy() {
    javaVM->DetachCurrentThread();
    env= nullptr;
    LOGV("Vm thread detached");
}
void* CodeResolver::runResolver(void *args) {
    bool isLog= false;
    CodeResolver*resolver = reinterpret_cast<CodeResolver*>(args);

    const char *clsName = dexGlobal.dexFile->getStringFromTypeIndex(resolver->methodId->class_idx_);
    if((unsigned long)(resolver->methodId)<1000){
        throw "Polluted method pointer";
    }
    const char *methodName=dexGlobal.dexFile->getStringByStringIndex(resolver->methodId->name_idx_);
    /*if(equals("Lcom/uc/shopping/ah;",clsName)){
        isLog= true;
        ::isLog= true;
    }*/

    char *sig = getProtoSig(resolver->methodId->proto_idx_, dexGlobal.dexFile);
    LOGV("Start Analysis,clsIdx=%u,class=%s,method=%s%s",
         resolver->methodId->class_idx_, clsName, methodName, sig);
    delete [] sig;
    resolver->initTries();
    const art::DexFile::CodeItem* code= resolver->codeItem;

    u4* defaultRegisters=new u4[code->registers_size_];
    resolver->initRegisters(defaultRegisters);

    JumpNode * curNode=new JumpNode(0,defaultRegisters);
    Range*lastRange =new Range(0);Range* nextRange= nullptr;

    u2 insns[code->insns_size_in_code_units_];
    memcpy(insns,code->insns_,2*code->insns_size_in_code_units_);
    u1 preData,opCode,*ins;u4 thisPos,lastPos=0, pos =0;
    //LOGV("Start run resolver size=%u registers=%u ",code->insns_size_in_code_units_,code->registers_size_);
    while(true){
        bool isNpeReturn=false;
        if(pos >= code->insns_size_in_code_units_){
            if(pos!=code->insns_size_in_code_units_){
                LOGW("Pos add wrong,pos=0x%x,cls=%s name=%s",pos,clsName,methodName);
            }
            Next:
            if(curNode->nextNode == nullptr){
                delete curNode;
                delete lastRange;
                goto EndPoint;
            }
            if(isNpeReturn){
                isNpeReturn= false;
                LOGV("into Npe");
                if(nextRange!= nullptr){
                    if(nextRange->preRange->preRange== nullptr){
                        LOGE("Unexpected npe at first range");
                        //roll back;temperary
                        pos=0;
                    } else{
                        nextRange->preRange=nextRange->preRange->preRange;
                        pos=nextRange->preRange->end;
                    }
                }
                else{
                    if(lastRange->preRange== nullptr){
                        //roll back;
                        pos=0;
                    } else{
                        lastRange=lastRange->preRange;
                        pos=lastRange->end;
                    }
                }
            }
            JumpNode* node=curNode;
            curNode=curNode->nextNode;
            delete node;
            Range* existed;
            if(isLog)LOGV("Try start new range");
            if((existed=Range::startNewRange(lastRange,nextRange,pos,curNode->ins_pos))== nullptr){
                pos =curNode->ins_pos;
                if(isLog) LOGV("%s Start new node at pos=%u",clsName,pos);
                continue;
            } else{
                if(isLog)LOGV("Move to next");
                pos=existed->end;
                nextRange=Range::seekNextRange(existed,lastRange);
                goto Next;
            }

        }
        try {
            Range::checkNextRange(pos, nextRange, lastRange);
        }catch (std::exception& e){
            LOGE("Range check failed ,lastOp=0x%x lastpos=0x%x lastpreData=0x%x class=%s m=%s",opCode,pos,preData,clsName,methodName);
            throw e;
        }

        ins= (u1 *) &insns[pos];
        opCode=*ins;
        preData=ins[1];
        thisPos = pos;
        if(isLog){
            LOGV("Op=0x%x pos=%u preData=0x%x",opCode,pos,preData);
            //usleep(20);
        }
        try {
            switch (opCode){
                case move:
                case moveOb:{
                    u1 rA= preData & (u1)0xf;
                    resolver->checkRegRange(rA);
                    resolver->checkRegRange(preData>>4);
                    curNode->registerTypes[rA]=curNode->registerTypes[preData >> 4];
                    pos++;
                    break;
                }
                case moveW:{
                    u1 rA= preData & (u1)0xf;
                    resolver->checkRegRange(rA+1);
                    resolver->checkRegRange(preData>>4);
                    curNode->registerTypes[rA]=curNode->registerTypes[preData >> 4];
                    curNode->registerTypes[rA+1]=TypePrimitive;
                    pos++;break;
                }
                case move16W:
                    resolver->checkRegRange(preData+1);
                    curNode->registerTypes[preData + 1]=TypePrimitive;
                case move16:
                case moveOb16:
                    resolver->checkRegRange(preData);
                    resolver->checkRegRange(insns[pos + 1]);
                    curNode->registerTypes[preData]=curNode->registerTypes[insns[pos + 1]];
                    pos +=2;break;
                case move16LW:
                    resolver->checkRegRange(insns[pos + 1] + 1);
                    curNode->registerTypes[insns[pos + 1] + 1]=TypePrimitive;
                case move16L:
                case moveOb16L:
                    resolver->checkRegRange(insns[pos + 1]);
                    resolver->checkRegRange(insns[pos + 2]);
                    curNode->registerTypes[insns[pos + 1]]=curNode->registerTypes[insns[pos + 2]];
                    pos +=3;break;
                case moveResultW:
                    resolver->checkRegRange(preData+1);
                    curNode->registerTypes[preData+1]=TypePrimitive;
                case moveResult:
                    resolver->checkRegRange(preData);
                    curNode->registerTypes[preData]=TypePrimitive;
                    ++pos;
                    break;
                case moveResultOb:{
                    ins=(u1 *) &insns[lastPos];
                    opCode= *ins;
                    //LOGV("%s last hex code before move result %x %x %x %x %x %x",clsName,ins[0],ins[1],ins[2],ins[3],ins[4],ins[5]);
                    switch(opCode){
                        case fillArray:
                        case fillArrayR:
                            resolver->checkRegRange(preData);
                            curNode->registerTypes[preData]=insns[lastPos + 1];//we can't judge it's array type
                            break;
                        default:{
                            const art::DexFile::MethodId& methodId=
                                    dexGlobal.dexFile->method_ids_[insns[lastPos + 1]];
                            //if(isLog)logMethod(methodId, dexGlobal.dexFile);
                            resolver->checkRegRange(preData);
                            curNode->registerTypes[preData]= dexGlobal.dexFile->
                                    proto_ids_[methodId.proto_idx_].return_type_idx_;
                            if(isLog)LOGV("%s Meet move result, last op=%d,reg %d updated with %d",clsName,opCode,preData,curNode->registerTypes[preData]);
                            break;
                        }
                    }
                    ++pos;break;
                }
                case moveExcept:++pos;break;
                case returnVNo:
                    insns[pos]=returnV;
                case returnV:
                case returnD:
                case returnW:
                case returnOb:
                case throwD:
                    //LOGV("meet return point %s %s",clsName,methodName);
                    goto Next;
                case const32:++pos;
                case const16:
                case const16H:pos+=2;
                    resolver->checkRegRange(preData);
                    curNode->registerTypes[preData]=TypePrimitive;
                    break;
                case const4:
                case arrayLen:++pos;
                    resolver->checkRegRange(preData & 0xfu);
                    curNode->registerTypes[u1(preData & 0xf)]=TypePrimitive;
                    break;
                case instanceOf:
                    resolver->checkRegRange(preData & 0xfu);
                    curNode->registerTypes[u1(preData & 0xf)]=TypePrimitive;
                    pos+=2;
                    break;
                case const64W:
                    pos +=2;
                case const32W:++pos;
                case const16W:
                case const16HW:
                    pos +=2;
                    resolver->checkRegRange(preData+1u);
                    curNode->registerTypes[preData]=TypePrimitive;
                    curNode->registerTypes[ preData + 1u]=TypePrimitive;
                    break;
                case constStrJ:++pos;
                case constStr:pos+=2;
                    if (globalRef.strTypeIdx == UNDEFINED)
                        throw std::runtime_error("String Type not initiated");
                    resolver->checkRegRange(preData);
                    curNode->registerTypes[preData]=globalRef.strTypeIdx;
                    break;
                case constClass:
                    if (globalRef.clsTypeIdx == UNDEFINED)
                        throw std::runtime_error("String Type not initiated");
                    resolver->checkRegRange(preData);
                    curNode->registerTypes[preData]=globalRef.clsTypeIdx;pos+=2;
                    break;
                case checkCast:
                case newInstance:
                    resolver->checkRegRange(preData);
                    curNode->registerTypes[preData]=insns[++pos];
                    ++pos;
                    break;
                case newArray:
                    if(isLog)LOGV("Meet new Array reg%u updated with %u real=%u",u1(preData & 0xf),TypeArray+insns[pos+1],insns[pos+1]);
                    resolver->checkRegRange(preData&0xfu);
                    curNode->registerTypes[u1(preData & 0xf)]=insns[pos+1];
                    pos+=2;
                    break;
                case fillArray:
                case fillArrayR:
                case fillArrayData:pos+=3;
                    break;
                case goto8:{
                    u4 newAddress=(u4)(pos+(int8_t) preData);
                    if(Range::startNewRange(lastRange,nextRange, pos, newAddress)== nullptr) {
                        // LOGV("%s goto %u",clsName,newAddress);
                        pos = newAddress;
                        break;
                    }
                    else goto Next;
                }

                case goto16:{
                    u4 newAddress=(u4)(pos +(int16_t) insns[pos+1]);
                    if(Range::startNewRange(lastRange,nextRange, ++pos, newAddress)== nullptr){
                        pos=newAddress;
                        break;
                    } else goto Next;
                }
                case goto32:{
                    u4 address=u4(pos+*((int32_t*)&insns[pos+1]));
                    if(Range::startNewRange(lastRange,nextRange, (pos+= 2), address)== nullptr){
                        pos = address;
                        break;
                    }else goto Next;
                }
                case packedSwitch:{
                    int offset=*((int*)&insns[pos+1]);
                    u4 tablePos=pos+offset;
                    u2 size = insns[tablePos + 1];
                    int* targets=((int*)(insns+tablePos))+2;
                    for(int i=0;i<size;++i){
                        forkNode(code, curNode, lastRange, thisPos + 2, pos + targets[i], clsName);
                    }
                    pos+=3;
                    break;
                }
                case sparseSwitch:{
                    int offset=*((int*)&insns[pos+1]);
                    u4 tablePos=pos+offset;
                    u2 size = insns[tablePos + 1];
                    int* targets=((int*)(insns+tablePos))+1+size;
                    for(int i=0;i<size;++i){
                        forkNode(code, curNode, lastRange, thisPos + 2, pos + targets[i], clsName);
                    }
                    pos+=3;
                    break;
                }
                case ifEq:
                case ifNe:
                case ifLt:
                case ifGe:
                case ifGt:
                case ifLe:
                case ifEqz:
                case ifNez:
                case ifLtz:
                case ifGez:
                case ifGtz:
                case ifLez:
                    forkNode(code, curNode, lastRange, pos + 1,
                             pos + (int16_t)/*avoid auto expand to u4*/insns[pos + 1], clsName);
                    pos+=2;
                    break;
                case agetW:
                case sgetW:
                    resolver->checkRegRange(preData+1);
                    curNode->registerTypes[preData+1]=TypePrimitive;
                case aget:
                case agetBoolean:
                case agetByte:
                case agetChar:
                case agetShort: {
                    //should I check npe?
                }
                case sget:
                case sgetBoolean:
                case sgetByte:
                case sgetChar:
                case sgetShort:{
                    resolver->checkRegRange(preData);
                    curNode->registerTypes[preData] = TypePrimitive;/*ignore primitive array as this operation has no effect on right code*/
                    pos+=2;break;
                }
                case igetW:
                    resolver->checkRegRange((preData&0xfu)+1);
                    curNode->registerTypes[(preData&0xf)+1]=TypePrimitive;
                case iget:
                case igetBoolean:
                case igetByte:
                case igetChar:
                case igetShort:{
                    u1 rOb= preData >> 4;
                    resolver->checkRegRange(rOb);
                    if(curNode->registerTypes[rOb]==TypePrimitive){
                        isNpeReturn= true;
                        goto Next;
                    }
                    resolver->checkRegRange(preData&0xfu);
                    curNode->registerTypes[preData&0xf]=TypePrimitive;
                    pos+=2;
                    break;
                }
                case agetOb:{
                    ins= (u1 *) &insns[pos + 1];
                    resolver->checkRegRange(*ins);
                    u4 arrayType=curNode->registerTypes[*ins];
                    if (arrayType == TypePrimitive) {
                        isNpeReturn = true;
                        goto Next;
                    }
                    u4 type = UNDEFINED;
                    const char* typeName=dexGlobal.dexFile->getStringFromTypeIndex(arrayType);
                    if(typeName[0]=='['){
                        if(strcmp(typeName+1,"Ljava/lang/String;")==0){
                            type=globalRef.strTypeIdx;
                        } else if(strcmp(typeName+1,"Ljava/lang/Class;")==0){
                            type=globalRef.clsTypeIdx;
                        } else{
                            auto dexFile = dexGlobal.dexFile;
                            binarySearchType(typeName + 1, type, dexFile);
                            if (type == UNDEFINED) {
                                LOGW("Can't find array component type woth name %s by binary search,loop find",
                                     typeName + 1);
                                // for unordered dexFile;
                                for (u4 i = 0, N = dexFile->header_->type_ids_size_; i < N; ++i) {
                                    if (strcmp(dexGlobal.dexFile->getStringFromTypeIndex(i),
                                               typeName + 1) == 0) {
                                        type = i;
                                        break;
                                    }
                                }
                            }
                        }
                    } else{
                        LOGE("Illegal array type %s",typeName);
                    }
                    if (type == UNDEFINED) {
                        //It may be the case that it's a temporary array multi-dimension array type.
                        LOGW("can't find class type for array type=%s", typeName);
                    }
                    //if(isLog) LOGV("Aget reg%u updated by reg%u with %u",preData,*ins,type);
                    resolver->checkRegRange(preData);
                    curNode->registerTypes[preData]=type;
                    pos+=2;break;
                }
                case igetOb:{
                    u1 rOb= preData >> 4;
                    resolver->checkRegRange(rOb);
                    if(curNode->registerTypes[rOb]==TypePrimitive){
                        isNpeReturn= true;
                        goto Next;
                    }
                    uint16_t typeIdx = dexGlobal.dexFile->
                            field_ids_[insns[pos+1]].type_idx_;
                    resolver->checkRegRange(preData&0xfu);
                    curNode->registerTypes[preData & 0xf]= typeIdx;
                    pos+=2;break;
                }
                case sgetOb:{
                    u4 type=dexGlobal.dexFile->
                            field_ids_[insns[pos+1]].type_idx_;
                    resolver->checkRegRange(preData);
                    curNode->registerTypes[preData]=type ;
                    pos+=2;break;
                }
                case iput:
                case iputW:
                case iputOb:
                case iputBoolean:
                case iputByte:
                case iputChar:
                case iputShort:
                {
                    u1 rOb= preData >> 4;
                    resolver->checkRegRange(rOb);
                    if(curNode->registerTypes[rOb]==TypePrimitive){
                        isNpeReturn= true;
                        goto Next;
                    }
                }
                case aput:
                case aputW:
                case aputOb:
                case aputBoolean:
                case aputByte:
                case aputChar:
                case aputShort: {
                    //should I check npe?
                }
                case sput:
                case sputW:
                case sputOb:
                case sputBoolean:
                case sputByte:
                case sputChar:
                case sputShort:{
                    pos+=2;
                    break;
                }
                case invokeVirtual:
                case invokeSuper:
                case invokeDirect:
                case invokeInterface:{
                    u1 iReg=u1(insns[pos+2]&0xf);
                    resolver->checkRegRange(iReg);
                    if(curNode->registerTypes[iReg]==TypePrimitive){
                        isNpeReturn= true;
                        //NullPointerException;
                        goto Next;
                    }
                    pos+=3;
                    break;
                }
                case invokeStatic:{pos+=3;break;}
                case invokeVirtualR:
                case invokeSuperR:
                case invokeDirectR:
                case invokeInterfaceR:{
                    resolver->checkRegRange(insns[pos+2]);
                    if(curNode->registerTypes[insns[pos+2]]==TypePrimitive){
                        isNpeReturn= true;
                        //NullPointerException;
                        goto Next;
                    }
                    pos+=3;
                    break;
                }
                case invokeStaticR:{pos+=3;break;}
#define CHECK_FIELD_NPE() u1 rOb= preData >> 4;\
                resolver->checkRegRange(rOb);\
                if(curNode->registerTypes[rOb]==TypePrimitive){\
                    isNpeReturn= true;\
                    goto Next;\
                }

#define IGET_CODE(x) CHECK_FIELD_NPE();\
                *ins=x;\
                resolver->alterField(curNode, insns, rOb, pos);\
                u1 rA=preData&u1(0xf);\
                resolver->checkRegRange(rA);\
                curNode->registerTypes[rA]=TypePrimitive;\
                pos+=2;break;

                case igetQ:{
                    IGET_CODE(iget)
                }
                case igetWQ:{
                    CHECK_FIELD_NPE();
                    *ins=igetW;
                    resolver->alterField(curNode, insns, rOb, pos);
                    u1 rA=preData&u1(0xf);
                    resolver->checkRegRange(rA+1);
                    curNode->registerTypes[rA]=TypePrimitive;
                    curNode->registerTypes[rA+1]=TypePrimitive;
                    pos+=2;break;
                }
                case igetObQ:{
                    CHECK_FIELD_NPE();
                    *ins=igetOb;
                    resolver->alterField(curNode, insns, rOb, pos);
                    u1 rA=preData&u1(0xf);
                    resolver->checkRegRange(rA);
                    curNode->registerTypes[rA]=
                            dexGlobal.dexFile->field_ids_[insns[pos+1]].type_idx_;
                    pos+=2;break;
                }
#define IPUT_CODE(x) CHECK_FIELD_NPE();\
                *ins=x;\
                resolver->alterField(curNode, insns, rOb, pos);\
                pos+=2;break;

                case iputQ:{
                    IPUT_CODE(iput);
                }
                case iputWQ:{
                    IPUT_CODE(iputW);
                }
                case iputObQ:{
                    IPUT_CODE(iputOb);
                }
                case invokeVirtualQ:{
                    u1 iReg=u1(insns[pos+2]&0xf);
                    resolver->checkRegRange(iReg);
                    if(curNode->registerTypes[iReg]==TypePrimitive){
                        isNpeReturn= true;
                        //NullPointerException;
                        goto Next;
                    }
                    *ins=invokeVirtual;
                    //LOGV("Meet invokeVirtualQ,regNum=%d",iReg);
                    resolver->checkRegRange(iReg);
                    u4 methodIdx= resolver->getVMethodFromIndex(curNode->registerTypes[iReg], insns[pos + 1]);
                    //if(isLog)LOGV("Got MethodIdx,idx=%u cls=%s",methodIdx,clsName);
                    if(methodIdx==UNDEFINED) {
                        goto Next;
                    }
                    resolver->changeIdx(insns, pos, methodIdx);
                    pos+=3;break;
                }
                case invokeVirtualRQ:{
                    resolver->checkRegRange(insns[pos+2]);
                    if(curNode->registerTypes[insns[pos+2]]==TypePrimitive){
                        isNpeReturn= true;
                        //NullPointerException;
                        goto Next;
                    }
                    *ins=invokeVirtualR;
                    u4 methodIdx= resolver->getVMethodFromIndex(curNode->registerTypes[insns[pos + 2]], insns[pos + 1]);
                    resolver->changeIdx(insns, pos, methodIdx);
                    pos+=3;break;
                }
                case iputBooleanQ:{
                    IPUT_CODE(iputBoolean);
                }
                case iputByteQ:{
                    IPUT_CODE(iputByte);
                }
                case iputCharQ:{
                    IPUT_CODE(iputChar);
                }
                case iputShortQ:{
                    IPUT_CODE(iputShort);
                }
                case igetBooleanQ: {
                    IGET_CODE(igetBoolean)
                }
                case igetByteQ:{
                    IGET_CODE(igetByte)
                }
                case igetCharQ:{IGET_CODE(igetChar)}
                case igetShortQ:{IGET_CODE(igetShort)}
                    //Lambda codes may affect some regs,but they will be
                    // reset before quick invoke some just skip them.
                case invokeLambda:{
                    pos+=2;
                    break;
                }
                case captureVariable:{
                    pos+=2;
                    break;
                }
                case createLambda:{
                    pos+=2;
                    break;
                }
                case liberateVariable:{
                    //For object type, it's still unimplemented
                    pos+=2;
                    break;
                }
                case boxLambda:{
                    pos+=2;
                    break;
                }
                case unboxLambda:{
                    pos+=2;
                    break;
                }
                case nop:{
                    switch (preData) {
                        case PACKED_SWITCH: {
                            u2 size = insns[pos + 1];
                            pos += ((size * 2) + 4);
                            break;
                        }
                        case SPARSE_SWITCH: {
                            u2 size = insns[pos + 1];
                            pos += ((size * 4) + 2);
                            break;
                        }
                        case FILL_ARRAY_DATA: {
                            u2 width = insns[pos + 1];
                            u4 size = *reinterpret_cast<u4 *>(&insns[pos + 2]);
                            u4 tableSize = (size * width + 1) / 2 + 4;//plus 1 in case width is odd,for even the plus is ignored
                            pos += tableSize;
                            break;
                        }
                        default: {
                            ++pos;
                            break;
                        }
                    }
                    break;
                }
                default:{
                    if((opCode>=0x2d&&opCode<=0x31)
                       ||(opCode>=0x90&&opCode<=0xaf)
                       ||(opCode>=0xd8&&opCode<=0xe2)){
                        curNode->registerTypes[preData]=TypePrimitive;
                        pos+=2;
                        break;
                    } else if(opCode>=0xd0&&opCode<=0xd7){
                        curNode->registerTypes[u1(preData&0xf)]=TypePrimitive;
                        pos+=2;
                        break;
                    }
                    ++pos;
                    break;
                }
            }
            TryItem* tryItem;
            if(resolver->tryMap != nullptr &&
               (tryItem= resolver->tryMap->seekTry(thisPos)) != nullptr){
                if(isLog){
                    LOGV("Into try dispatch,try pos=%u",lastPos);
                    for(int j=0;j<tryItem->handlerSize;++j){
                        LOGV("Handler%d at pos %u",j,tryItem->handlers[j].offset);
                    }
                }
                for(int j=0;j<tryItem->handlerSize;++j){
                    u4 offset=tryItem->handlers[j].offset;
                    if(forkNode(code, curNode, lastRange, pos - 1, offset+1/*the move exception code is skipped*/, clsName)){
                        if(*((u1*)&insns[offset])!=moveExcept){
                            LOGE("Move Exception unexpected ,offset=%u op=%u,pre=%u,handler pos=%d",offset,*((u1*)&insns[offset]),(u1) (insns[offset] >> 8),j);
                            throw std::out_of_range("std::out_of_range:moveExcept");
                        }
                        u1 reg= (u1) (insns[offset] >> 8);/*the move exception code*/
                        resolver->checkRegRange(reg);
                        curNode->nextNode->registerTypes[reg]=tryItem->handlers[j].typeIdx;/*pre set exception type*/
                    }

                }
            }
            lastPos=thisPos;
        } catch (std::exception &e) {
            LOGE("Meet exception %s,Op=0x%x pos=0x%x preData=0x%x,cls=%s,m=%s", e.what(), opCode,
                 pos, preData, clsName, methodName);
            throw e;
        }


    }
    EndPoint:
        if(isLog)LOGV("Goto end point");
    int fd=open(dexGlobal.dexFileName,O_WRONLY);
    pwrite(fd,insns,(u8)code->insns_size_in_code_units_<<1,resolver->fileOffset);
    close(fd);
    if(isLog)LOGV("Write insns Over");
    delete resolver;
    //LOGV("Resolver deleted %s %s",clsName ,methodName);
    return nullptr;
}

void CodeResolver::binarySearchType(const char *typeName, u4 &type, const art::DexFile *dexFile) {
    u4 low = 0, high = dexFile->header_->type_ids_size_ - 1, mid;
    int value;
    while (low <= high) {
        mid = (low + high) >> 1;
        auto name = dexFile->getStringFromTypeIndex(mid);
        value = dexUtf8Cmp(name, typeName);
        if (value == 0) {
            type = mid;
            break;
        } else if (value > 0) {
            high = mid - 1;
        } else low = mid + 1;
    }
}

void CodeResolver::alterField(const CodeResolver::JumpNode *curNode,
                               u2 *insns, u1 rOb, u4 pos) {
    //if(isLog)LOGV("Start get field offset rOb=%u,typeIdx=%u",rOb,curNode->registerTypes[rOb]);
    u4 fieldIdx =getFiledFromOffset(curNode->registerTypes[rOb], insns[pos + 1]);
    //if(isLog)LOGV("Field index Gotten %d",fieldIdx);
    changeIdx(insns, pos, fieldIdx);
}

void CodeResolver::changeIdx( u2 *insns, u4 pos, u4 Idx) const {
    if(Idx == UNDEFINED){
        char *sig = getProtoSig(methodId->proto_idx_, dexGlobal.dexFile);
        LOGW("Unable to find index at pos%u;class=%s,method=%s%s",pos,
             dexGlobal.dexFile->getStringFromTypeIndex(methodId->class_idx_),
             dexGlobal.dexFile->getStringByStringIndex(methodId->name_idx_),sig);
        delete[] sig;
    }else insns[pos+1]= (u2) Idx;
}

bool CodeResolver::forkNode(const art::DexFile::CodeItem *code, JumpNode *curNode, Range*lastRange, u4 lastPos,
                            u4 newPos, const char* curClass) {
    if(newPos>code->insns_size_in_code_units_){
        LOGE("invalid new pos=%u at pos=%u cls=%s", newPos, lastPos, curClass);
        throw std::out_of_range("std::out_of_range:newPos");
    }
    if(Range::checkRange(lastRange, newPos)){
        //LOGV("New Node Forked from %u to %u",lastPos,newPos);
        u4* regs=new u4[code->registers_size_];
        memcpy(regs,curNode->registerTypes, sizeof(u4)*code->registers_size_);
        JumpNode* nextNode=new JumpNode(newPos, regs, curNode->nextNode);
        curNode->nextNode=nextNode;
        return true;
    }
    return false;
}

bool CodeResolver::pend() {
    if(codeItem== nullptr){
        LOGE("The resolver are not initiated, init it at first");
        return false;
    }
    if(globalRef.curDexFile== nullptr||globalRef.curDexFile!=dexGlobal.dexFile){
        globalRef.curDexFile=dexGlobal.dexFile;
        globalRef.strTypeIdx=UNDEFINED;
        globalRef.clsTypeIdx=UNDEFINED;
        //String type is more possible to appear than class type
        binarySearchType("Ljava/lang/String;", globalRef.strTypeIdx, globalRef.curDexFile);
        if (globalRef.strTypeIdx == UNDEFINED) {
            for (u4 i = 0, N = dexGlobal.dexFile->header_->type_ids_size_; i < N && (globalRef.
                    strTypeIdx == UNDEFINED || globalRef.clsTypeIdx == UNDEFINED); ++i) {
                auto className = dexGlobal.dexFile->getStringFromTypeIndex(i);
                if (strcmp(className,
                           "Ljava/lang/String;") == 0) {
                    globalRef.strTypeIdx = i;
                }
                if (strcmp(className,
                           "Ljava/lang/Class;") == 0) {
                    globalRef.clsTypeIdx = i;
                }
            }
        } else {
            for (u4 i = globalRef.strTypeIdx; i != -1; --i) {
                if (strcmp(dexGlobal.dexFile->getStringFromTypeIndex(i),
                           "Ljava/lang/Class;") == 0) {
                    globalRef.clsTypeIdx = i;

                    break;
                }
            }
        }
        LOGV("meet str type idx=%d", globalRef.strTypeIdx);
        LOGV("meet class type idx=%d", globalRef.clsTypeIdx);
    }
    dexGlobal.initPoolIfNeeded(runResolver,threadInit,threadDestroy);
    dexGlobal.pool->submit(this);

    return true;
}

void CodeResolver:: initTries() {
    if(codeItem->tries_size_==0) return;
    TryItem* tries= new TryItem[codeItem->tries_size_];
    int size;
    u1* tryStart= (u1*)codeItem+(int)sizeof(art::DexFile::CodeItem) - 2 + codeItem->insns_size_in_code_units_ * 2;
    if((codeItem->insns_size_in_code_units_&1) ==1)tryStart+=2;
    const u1* handlerStart= tryStart + (int)sizeof(art::DexFile::TryItem) * codeItem->tries_size_;
    for(int i=0;i<codeItem->tries_size_;++i){
        art::DexFile::TryItem* dexTryItem= reinterpret_cast<art::DexFile::TryItem*>(
                tryStart+i* sizeof(art::DexFile::TryItem));
        TryItem* tryItem= &tries[i];
        tryItem->pos=dexTryItem->start_addr_+dexTryItem->insn_count_-1;
        const u1* ptr=handlerStart+dexTryItem->handler_off_;
        int hCount=readSignedLeb128(size,ptr);
        bool hasCatchAll=hCount<=0;
        if(hCount<0)hCount=-hCount;
        tryItem->handlerSize= (u4) (hasCatchAll + hCount);
        tryItem->handlers=new Handler[tryItem->handlerSize];

        for(int j=0;j<hCount;++j){
            int typeIdx=readUnsignedLeb128(size,ptr);
            int address=readUnsignedLeb128(size,ptr);
            if((u4)address>codeItem->insns_size_in_code_units_){
                LOGE("Bad try handler address=%d,max=%u",address,codeItem->insns_size_in_code_units_) ;
                throw std::out_of_range("try handler");
            }
            tryItem->handlers[j].typeIdx= (u4) typeIdx;
            tryItem->handlers[j].offset= (u4) address;

        }
        if(hasCatchAll){
            tryItem->handlers[hCount].typeIdx=TypeException;
            tryItem->handlers[hCount].offset= (u4) readUnsignedLeb128(size,ptr);
        }
    }
    tryMap=new TryMap(tries,codeItem->tries_size_);
}
CodeResolver::TryItem* CodeResolver::TryMap::seekTry(u4 pos) {
    if(pos<start||pos>end) return nullptr;
    if(isMapped){
        TryItem* tryItem=tryMap[(pos-start)/div];
        if(tryItem!= nullptr&&pos==tryItem->pos){
            return tryItem;
        }
    } else{
        int low=0,mid=0,high=size-1;
        while (low<=high){
            mid=(low+high)>>1;
            TryItem& tryItem=tries[mid];
            if(tryItem.pos>pos){
                high=mid-1;
            } else if(tryItem.pos==pos){
                return &tryItem;
            } else low=mid+1;
        }
    }
    return nullptr;
}
void CodeResolver::initRegisters(u4* registers) {
    int paraSize=protoList== nullptr?0:protoList->Size();
    //LOGV("Into init registers,registerSize=%d,paraSize=%d,isInstance=%d",codeItem->registers_size_,paraSize,isInstance);

    int paraStart=codeItem->registers_size_-paraSize;

    u4 i=0;
    for(;i<paraStart;++i){
        registers[i]=UNDEFINED;
    }
    if(isInstance) {
        registers[codeItem->registers_size_ - paraSize - 1] = methodId->class_idx_;
    }
    for(i=0;i<paraSize;++i){
        registers[paraStart+i]=protoList->GetTypeItem(i).type_idx_;
    }
}
u4 CodeResolver::getVMethodFromIndex(u4 clsIdx, u4 vIdx) {
    if(clsIdx==TypeException){
        LOGW("Unexpected type TypeException,as catch all has no type specified");
        return UNDEFINED;
    }
    //if(isLog)LOGV("Vmethod classIdx=%u,vIdx=%x",clsIdx,vIdx);
    const char*clsName =dexGlobal.dexFile->getStringFromTypeIndex(clsIdx);
    if (clsName[0] != 'L' && clsName[0] !=
                             '[') {//Array type is namely sub-type of object,so inherit all the virtual methods of object
        LOGE("Invalid class Foundm name=%s c=%s ", clsName,
             dexGlobal.dexFile->getStringFromTypeIndex(methodId->class_idx_));
        return UNDEFINED;
    }
    char *cClassName = toJavaClassName(clsName);
    if (isLog)LOGV("Vmethod cls name=%s", cClassName);
    jstring javaClassName = env->NewStringUTF(cClassName);
    jobject javaMethod= env->CallStaticObjectMethod(dexGlobal.getToolsClass(), dexGlobal.getGetMethodID(), javaClassName,(jint) vIdx);
    env->DeleteLocalRef(javaClassName);
    delete[] cClassName;
    if(javaMethod==NULL)
        return UNDEFINED;
    jbyteArray result = (jbyteArray) env->CallStaticObjectMethod(dexGlobal.getToolsClass(),
                                                                 dexGlobal.getConvertMember(),
                                                                 javaMethod);
    env->DeleteLocalRef(javaMethod);
    jboolean isCopy;
    char *cStr = (char *) env->GetPrimitiveArrayCritical(result, &isCopy);
    char temp[strlen(cStr) + 1];
    char *cls = temp;
    memcpy(cls,cStr,strlen(cStr)+1);
    env->ReleasePrimitiveArrayCritical(result, cStr, 0);
    char *mName, *retType, *proto;
    env->DeleteLocalRef(result);
    char* sp=strchr(cls,'|');
    *sp='\0'; mName=sp+1;
    sp=strchr(mName,'|');
    *sp = '\0';
    retType = sp + 1;
    sp = strchr(retType, '|');
    *sp='\0';proto=sp+1;
    if (isLog)
        LOGV("Start compare method in dex,cls=%s,name=%s,sig=(%s)%s", cls, mName, proto, retType);

    u4 low = 0, high = dexGlobal.dexFile->header_->method_ids_size_ - 1, middle;
    int value;
    value = dexUtf8Cmp(cls, clsName);
    bool isTheSame = false;
    bool exchanged = false;
    if (value > 0) {
        char *tmp = cls;
        cls = (char *) clsName;
        clsName = tmp;
        exchanged = true;
    } else if (value == 0) isTheSame = true;
    while (low <= high) {
        middle = (low + high) >> 1;
        const art::DexFile::MethodId *mid = &dexGlobal.dexFile->method_ids_[middle];
        const char *thizClass = dexGlobal.dexFile->getStringFromTypeIndex(mid->class_idx_);

#define HandleZero(className)  value=dexUtf8Cmp(thizClass,className);\
        if(value==0){\
            value=dexUtf8Cmp(dexGlobal.dexFile->getStringByStringIndex(mid->name_idx_),mName);\
            if(value==0){ \
                const art::DexFile::ProtoId& protoId= dexGlobal.dexFile->proto_ids_[mid->proto_idx_];\
                value=dexUtf8Cmp(dexGlobal.dexFile->getStringFromTypeIndex(protoId.return_type_idx_),retType);\
                if(value==0){\
                    std::string protoString;\
                    getProtoString(protoId,dexGlobal.dexFile,protoString);\
                    value=dexUtf8Cmp(&protoString[0],proto);\
                    if(value==0)\
                        return middle;\
                }\
            }\
        }

        HandleZero(cls);
        if (value < 0) {
            low = middle + 1;
        } else {
            if (isTheSame) {
                high = middle - 1;
                continue;
            }
            HandleZero(clsName);
            if (value > 0) {
                high = middle - 1;
            } else {
                u4 mid1 = middle, high1 = high;
                high = middle - 1;
#define MiddleSeek(className)\
                while (low<=high){\
                    middle=(low+high)>>1;\
                    mid=&dexGlobal.dexFile->method_ids_[middle];\
                    thizClass=dexGlobal.dexFile->getStringFromTypeIndex(mid->class_idx_);\
                    HandleZero(className);\
                    if(value<0){\
                        low=middle+1;\
                    } else{\
                        high=middle-1;\
                    }\
                }

                MiddleSeek(cls);
                low = mid1 + 1;
                high = high1;
                MiddleSeek(clsName);
#undef MiddleSeek
#undef HandleZero
                break;
            }
        }
    }

    if (exchanged) cls = (char *) clsName;
    *(mName - 1) = '|';
    *(retType - 1) = '|';
    *(proto - 1) = '|';
    LOGE("Can't find method id for %s in ordered mode,start full glance", cls);
    *(mName - 1) = '\0';
    *(retType - 1) = '\0';
    *(proto - 1) = '\0';

    for (u4 i = 0; i < dexGlobal.dexFile->header_->method_ids_size_; ++i) {
          const art::DexFile::MethodId& mid=dexGlobal.dexFile->method_ids_[i];
          const char* thizClass=dexGlobal.dexFile->getStringFromTypeIndex(mid.class_idx_);
          if(strcmp(cls,thizClass)!=0&&strcmp(clsName,thizClass)!=0) continue;
          const char* name=dexGlobal.dexFile->getStringByStringIndex(mid.name_idx_);
          if(strcmp(name,mName)!=0) continue;
          std::string protoString;
          getProtoString(dexGlobal.dexFile->proto_ids_[mid.proto_idx_],dexGlobal.dexFile,protoString);
          if(protoString.compare(proto)==0){
              return i;
          }
    }
    *(mName - 1) = '|';
    *(retType - 1) = '|';
    *(proto - 1) = '|';
    LOGE("Can't find method id for %s", cls);
    return UNDEFINED;
}
u4 CodeResolver::getFiledFromOffset(u4 clsIdx, u4 fieldOffset) {
    const char*clsName =dexGlobal.dexFile->getStringFromTypeIndex(clsIdx);
    if (clsName[0] != 'L') {//only object type has field.
        LOGE("Qfield Invalid class Found f=%s,classIdx=%u,vIdx=%x",
             dexGlobal.dexFile->getStringFromTypeIndex(methodId->class_idx_), clsIdx, fieldOffset);
    }
    const char *cClassName = toJavaClassName(clsName);

    jstring javaClassName = env->NewStringUTF(cClassName);
    jobject javaFiled= env->CallStaticObjectMethod(dexGlobal.getToolsClass(),dexGlobal.getGetFieldID(), javaClassName,(jint) fieldOffset);
    env->DeleteLocalRef(javaClassName);
    delete[] cClassName;
    if(javaFiled==NULL)
        return UNDEFINED;
    if(isLog)LOGV("Start find field loop ");
    jbyteArray result = (jbyteArray) env->CallStaticObjectMethod(dexGlobal.getToolsClass(),
                                                                 dexGlobal.getConvertMember(),
                                                                 javaFiled);
    env->DeleteLocalRef(javaFiled);
    jboolean isCopy;
    char *cStr = (char *) env->GetPrimitiveArrayCritical(result, &isCopy);
    char temp[strlen(cStr) + 1];
    char *cls = temp;
    char *fName,*typeName;
    memcpy(cls,cStr,strlen(cStr)+1);
    env->ReleasePrimitiveArrayCritical(result, cStr, 0);
    env->DeleteLocalRef(result);
    char* sp=strchr(cls,'|');
    *sp='\0'; fName=sp+1;
    sp=strchr(fName,'|');
    *sp='\0';typeName=sp+1;
    //LOGV("Start find field in dex,%s %s %s",cls,fName,typeName);

    u4 low = 0, high = dexGlobal.dexFile->header_->field_ids_size_ - 1, middle;
    int value;
    value = dexUtf8Cmp(cls, clsName);
    bool isTheSame = false;
    bool exchanged = false;
    if (value > 0) {
        char *tmp = cls;
        cls = (char *) clsName;
        clsName = tmp;
        exchanged = true;
    } else if (value == 0) isTheSame = true;
    while (low <= high) {
        middle = (low + high) >> 1;
        const art::DexFile::FieldId *fid = &dexGlobal.dexFile->field_ids_[middle];
        const char *thizClass = dexGlobal.dexFile->getStringFromTypeIndex(fid->class_idx_);

#define HandleZero(className)  value=dexUtf8Cmp(thizClass,className);\
        if(value==0){\
            const char* name=dexGlobal.dexFile->getStringByStringIndex(fid->name_idx_);\
            value=dexUtf8Cmp(name,fName);\
            if(value==0){\
                const char* type=dexGlobal.dexFile->getStringFromTypeIndex(fid->type_idx_);\
                value=dexUtf8Cmp(type,typeName);\
                if(value!=0)\
                    LOGE("Type Mismatch field%s.%s:%s,should be type:%s",cls,name,typeName,typeName);\
                else\
                    return middle;\
            }\
        }

        HandleZero(cls);
        if (value < 0) {
            low = middle + 1;
        } else {
            if (isTheSame) {
                high = middle - 1;
                continue;
            }
            HandleZero(clsName);
            if (value > 0) {
                high = middle - 1;
            } else {
                u4 mid1 = middle, high1 = high;
                high = middle - 1;
#define MiddleSeek(className)\
                while (low<=high){\
                    middle=(low+high)>>1;\
                    fid=&dexGlobal.dexFile->field_ids_[middle];\
                    thizClass=dexGlobal.dexFile->getStringFromTypeIndex(fid->class_idx_);\
                    HandleZero(className);\
                    if(value<0){\
                        low=middle+1;\
                    } else{\
                        high=middle-1;\
                    }\
                }

                MiddleSeek(cls);
                low = mid1 + 1;
                high = high1;
                MiddleSeek(clsName);
#undef MiddleSeek
#undef HandleZero
                break;
            }
        }
    }
    if (exchanged) cls = (char *) clsName;
    *(typeName - 1) = '|';
    *(fName - 1) = '|';
    LOGE("Can't find field id for %s by binary search start full glance", cls);
    *(typeName - 1) = '\0';
    *(fName - 1) = '\0';

    for (u4 i = 0; i < dexGlobal.dexFile->header_->field_ids_size_; ++i) {
        const art::DexFile::FieldId &fid = dexGlobal.dexFile->field_ids_[i];
        const char *thizClass = dexGlobal.dexFile->getStringFromTypeIndex(fid.class_idx_);
        if (strcmp(cls, thizClass) != 0 && strcmp(thizClass, clsName) != 0)
            continue;
        const char *name = dexGlobal.dexFile->getStringByStringIndex(fid.name_idx_);
        if (strcmp(name, fName) != 0) continue;
        const char *type = dexGlobal.dexFile->getStringFromTypeIndex(fid.type_idx_);
        if (strcmp(type, typeName) != 0)
            LOGW("Type Mismatch field%s.%s:%s,should be type:%s", cls, name, typeName, typeName);
        else {
            return i;
        }
    }
    *(typeName - 1) = '|';
    *(fName - 1) = '|';
    LOGE("Can't find field id for %s", cls);
    return UNDEFINED;
}


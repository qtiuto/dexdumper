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
        LOGW("Register native method failed");
        javaVM->DetachCurrentThread();
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

    const char *clsName=getStringFromStringId(dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->type_ids_
    [resolver->methodId->class_idx_].descriptor_idx_],dexGlobal.dexFile->begin_);
    const char *methodName=getStringFromStringId(dexGlobal.dexFile->string_ids_[resolver->methodId->name_idx_]
            ,dexGlobal.dexFile->begin_);
    /*if(equals("Lcom/tencent/open/b/a;",clsName)&&equals(clsName
            ,"Lcom/shenzhoufu/szfpaymentbycredit/main/MainBalanceNotEnough;")
            &&equals("Lx/y/a/dm;",clsName)) isLog= true;*/
    ::isLog=isLog;

    char* sig=getProtoSig(dexGlobal.dexFile->proto_ids_[resolver->methodId->proto_idx_],dexGlobal.dexFile->type_ids_,dexGlobal.dexFile->string_ids_,dexGlobal.dexFile->begin_);
    LOGV("Start AnalysisclsIdx=%u,class=%s,method=%s%s", resolver->thizTypeIdx, getStringFromStringId(dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->
            type_ids_[resolver->thizTypeIdx].descriptor_idx_], dexGlobal.dexFile->begin_), getStringFromStringId(dexGlobal.dexFile->
            string_ids_[resolver->methodId->name_idx_], dexGlobal.dexFile->begin_), sig);
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
                //LOGV("End");
                delete curNode;
                //LOGV("End delete Node");
                delete lastRange;
               // LOGV("End Delete Range");
                goto EndPoint;
            }
            if(isNpeReturn){
                isNpeReturn= false;
                LOGV("into Npe");
                if(nextRange!= nullptr){
                    if(nextRange->preRange->preRange== nullptr){
                        LOGW("Unexpected npe at first range");
                        //roll back;temperory
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
                if(isLog) LOGV("%s Start new node at pos=%u",clsName,pos);
                pos =curNode->ins_pos;
                continue;
            } else{
                if(isLog)LOGV("Move to next");
                pos=existed->end;
                nextRange=Range::seekNextRange(existed,lastRange);
                goto Next;
            }

        }
        if(isLog){LOGV("Check Next Range");}
        Range::checkNextRange(pos, nextRange, lastRange);
        ins= (u1 *) &insns[pos];
        opCode=*ins;
        preData=ins[1];
        thisPos = pos;
        if(isLog){
            LOGV("Op=0x%x pos=0x%x preData=0x%x",opCode,pos,preData);
            //usleep(20);
        }
        switch (opCode){
            case move:
            case moveOb:{
                u1 rA= preData & (u1)0xf;
                curNode->registerTypes[rA]=curNode->registerTypes[preData >> 4];
                pos++;
                break;
            }
            case moveW:{
                u1 rA= preData & (u1)0xf;
                curNode->registerTypes[rA]=curNode->registerTypes[preData >> 4];
                curNode->registerTypes[rA+1]=TypePrimitive;
                pos++;break;
            }
            case move16W:
                curNode->registerTypes[preData + 1]=TypePrimitive;
            case move16:
            case moveOb16:
                curNode->registerTypes[preData]=curNode->registerTypes[insns[pos + 1]];
                //if(isLog) LOGV("Reg%d updated by reg%d with %u",preData,insns[pos+1],curNode->registerTypes[insns[pos + 1]]);
                pos +=2;break;
            case move16LW:
                curNode->registerTypes[insns[pos + 1] + 1]=TypePrimitive;
            case move16L:
            case moveOb16L:
                curNode->registerTypes[insns[pos + 1]]=curNode->registerTypes[insns[pos + 2]];
                pos +=3;break;
            case moveResultW:
                curNode->registerTypes[preData+1]=TypePrimitive;
            case moveResult:
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
                        curNode->registerTypes[preData]=insns[lastPos + 1];//we can't judge it's array type
                        break;
                    default:{
                        const art::DexFile::MethodId& methodId=
                                dexGlobal.dexFile->method_ids_[insns[lastPos + 1]];
                        if(isLog)logMethod(methodId, dexGlobal.dexFile);
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
                curNode->registerTypes[preData]=TypePrimitive;
                break;
            case const4:
            case arrayLen:++pos;
                curNode->registerTypes[u1(preData & 0xf)]=TypePrimitive;
                break;
            case instanceOf:
                curNode->registerTypes[u1(preData & 0xf)]=TypePrimitive;
                pos+=2;
                break;
            case const64W:
                pos +=2;
            case const32W:++pos;
            case const16W:
            case const16HW:
                pos +=2;
                curNode->registerTypes[preData]=TypePrimitive;
                curNode->registerTypes[(u4) preData + 1]=TypePrimitive;
                break;
            case constStrJ:++pos;
            case constStr:pos+=2;
                curNode->registerTypes[preData]=globalRef.strTypeIdx;
                break;
            case constClass:
                curNode->registerTypes[preData]=globalRef.clsTypeIdx;pos+=2;
                break;
            case checkCast:
            case newInstance:
                curNode->registerTypes[preData]=insns[++pos];
                ++pos;
                break;
            case newArray:
                if(isLog)LOGV("Meet new Array reg%u updated with %u real=%u",u1(preData & 0xf),TypeArray+insns[pos+1],insns[pos+1]);
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
                //LOGV("Meet packed switch size=%u offset=%d",size,offset);
                for(int i=0;i<size;++i){
                    if(forkNode(code, curNode, lastRange, thisPos + 2, pos + targets[i], clsName)){
                        //LOGV("%s Target at %d=%u",clsName,i,pos+targets[i]);
                    }
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
                forkNode(code, curNode, lastRange, pos + 1, pos + (int16_t)insns[pos + 1], clsName);
                //LOGV(" At pos =%u if new node fork offset= %d",pos,(int16_t) insns[pos + 1]);
                pos+=2;
                break;
            case agetW:
            case sgetW:
                curNode->registerTypes[preData+1]=TypePrimitive;
            case aget:
            case agetBoolean:
            case agetByte:
            case agetChar:
            case agetShort:
            case sget:
            case sgetBoolean:
            case sgetByte:
            case sgetChar:
            case sgetShort:{
                curNode->registerTypes[preData]=TypePrimitive;
                pos+=2;break;
            }
            case igetW:
                curNode->registerTypes[(preData&0xf)+1]=TypePrimitive;
            case iget:
            case igetBoolean:
            case igetByte:
            case igetChar:
            case igetShort:{
                u1 rOb= preData >> 4;
                if(curNode->registerTypes[rOb]==TypePrimitive){
                    isNpeReturn= true;
                    goto Next;
                }
                curNode->registerTypes[preData&0xf]=TypePrimitive;
                pos+=2;
                break;
            }
            case agetOb:{
                ins= (u1 *) &insns[pos + 1];
                u4 arrayType=curNode->registerTypes[*ins];
                u4 type=0;
                const char* typeName=getStringFromStringId(
                        dexGlobal.dexFile->string_ids_[
                                dexGlobal.dexFile->type_ids_[arrayType].
                                        descriptor_idx_],dexGlobal.dexFile->begin_);
                if(typeName[0]=='['){
                    if(strcmp(typeName+1,"Ljava/lang/String;")==0){
                        type=globalRef.strTypeIdx;
                    } else if(strcmp(typeName+1,"Ljava/lang/Class;")==0){
                        type=globalRef.clsTypeIdx;
                    } else{
                        const art::DexFile* dexFile=dexGlobal.dexFile;
                        for(u4 i=0,N=dexFile->header_->type_ids_size_;i<N;++i){
                            if(strcmp(getStringFromStringId(dexFile->string_ids_
                                                            [dexFile->type_ids_[i].descriptor_idx_],dexFile->begin_),
                                      typeName+1)==0){
                                type=i;
                                break;
                            }
                        }
                    }
                } else{
                    LOGW("Illegal array type %s",typeName);
                }
                if(type== 0){
                    LOGW("can't find class type for array type=%s",typeName);
                }
                //if(isLog) LOGV("Aget reg%u updated by reg%u with %u",preData,*ins,type);
                curNode->registerTypes[preData]=type;
                pos+=2;break;
            }
            case igetOb:{
                u1 rOb= preData >> 4;
                if(curNode->registerTypes[rOb]==TypePrimitive){
                    isNpeReturn= true;
                    goto Next;
                }
                uint16_t typeIdx = dexGlobal.dexFile->
                        field_ids_[insns[pos+1]].type_idx_;
                //LOGV("reg=%u, updated with %u, referred by data %u",preData&0xf,typeIdx,insns[pos+1]);
                //usleep(50);
                curNode->registerTypes[preData & 0xf]= typeIdx;
                pos+=2;break;
            }
            case sgetOb:{
                u4 type=dexGlobal.dexFile->
                        field_ids_[insns[pos+1]].type_idx_;
                //LOGV("reg=%u, updated with %u, referred by data %u",preData,type,insns[pos+1]);
                //usleep(50);
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
            case aputShort:
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
                if(curNode->registerTypes[iReg]==TypePrimitive){
                    isNpeReturn= true;
                    //NullPointerException;
                    LOGV("npe met");
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
                if(curNode->registerTypes[rOb]==TypePrimitive){\
                    isNpeReturn= true;\
                    goto Next;\
                }

#define IGET_CODE(x) CHECK_FIELD_NPE();\
                *ins=x;\
                resolver->alterField(curNode, insns, rOb, pos);\
                u1 rA=preData&u1(0xf);\
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
                curNode->registerTypes[rA]=TypePrimitive;
                curNode->registerTypes[rA+1]=TypePrimitive;
                pos+=2;break;
            }
            case igetObQ:{
               CHECK_FIELD_NPE();
                *ins=igetOb;
                resolver->alterField(curNode, insns, rOb, pos);
                u1 rA=preData&u1(0xf);
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
                if(curNode->registerTypes[iReg]==TypePrimitive){
                    isNpeReturn= true;
                    //NullPointerException;
                    goto Next;
                }
                *ins=invokeVirtual;
                //LOGV("Meet invokeVirtualQ,regNum=%d",iReg);
                u4 methodIdx= resolver->getVMethodFromIndex(curNode->registerTypes[iReg], insns[pos + 1]);
                if(isLog)LOGV("Got MethodIdx,idx=%u cls=%s",methodIdx,clsName);
                if(methodIdx==UNDEFINED) {
                    goto Next;
                }
                resolver->changeIdx(insns, pos, methodIdx);
                pos+=3;break;
            }
            case invokeVirtualRQ:{
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
            //JumpNode* node=curNode->nextNode;
            if(isLog)LOGV("Into try dispatch");
            for(int j=0;j<tryItem->handlerSize;++j){
                if(forkNode(code, curNode, lastRange, pos - 1, tryItem->handlers[j].offset + 1, clsName)){
                    u1 reg= (u1) (insns[curNode->nextNode->ins_pos] >> 8);
                    curNode->nextNode->registerTypes[reg]=tryItem->handlers[j].typeIdx;
                }

            }
        }
        lastPos=thisPos;
    }
    EndPoint:
        if(isLog)LOGV("Goto end point");
    int fd=open(dexGlobal.dexFileName,O_WRONLY);
    //lseek(fd,resolver->fileOffset,SEEK_SET);
    pwrite(fd,insns,(u8)code->insns_size_in_code_units_<<1,resolver->fileOffset);
    close(fd);
    if(isLog)LOGV("Write insns Over");
    //delete [] insns;
    //LOGV("Loop Over end analysis,%p %p %p %p",resolver,resolver->tryMap,resolver->toolsClass,resolver->env);
    delete resolver;
     LOGV("Resolver Deleted");
    //LOGV("Resolver deleted %s %s",clsName ,methodName);
    //usleep(5000);
    //LOGV("Vm thread to be detached %s %s",clsName,methodName);
    return nullptr;
}

void CodeResolver::alterField(const CodeResolver::JumpNode *curNode,
                               u2 *insns, u1 rOb, u4 pos) {
    if(isLog)LOGV("Start get field offset rOb=%u,typeIdx=%u",rOb,curNode->registerTypes[rOb]);
    u4 fieldIdx =getFiledFromOffset(curNode->registerTypes[rOb], insns[pos + 1]);
    if(isLog)LOGV("Field index Gotten %d",fieldIdx);
    changeIdx(insns, pos, fieldIdx);

}

void CodeResolver::changeIdx( u2 *insns, u4 pos, u4 Idx) const {
    if(Idx == UNDEFINED){
        char* sig=getProtoSig(dexGlobal.dexFile->proto_ids_[methodId->proto_idx_],dexGlobal.dexFile->type_ids_,dexGlobal.dexFile->string_ids_,dexGlobal.dexFile->begin_);
        LOGW("Unable to find index at pos%u;class=%s,method=%s%s",pos, getStringFromStringId(dexGlobal.dexFile->
                string_ids_[dexGlobal.dexFile->type_ids_[methodId->class_idx_].descriptor_idx_],dexGlobal.dexFile->begin_), getStringFromStringId(
                dexGlobal.dexFile->string_ids_[methodId->name_idx_],dexGlobal.dexFile->begin_),sig);
        delete[] sig;
    }else insns[pos+1]= (u2) Idx;
}

bool CodeResolver::forkNode(const art::DexFile::CodeItem *code, JumpNode *curNode, Range*lastRange, u4 lastPos,
                            u4 newPos, const char* curClass) {
    if(newPos>code->insns_size_in_code_units_){
        LOGW("invalid new pos=%u at pos=%u cls=%s",newPos,lastPos,curClass);
        return false;
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
        LOGW("The resolver are not initiated, init it at first");
        return false;
    }
    if(globalRef.curDexFile== nullptr||globalRef.curDexFile!=dexGlobal.dexFile){
        globalRef.curDexFile=dexGlobal.dexFile;
        globalRef.strTypeIdx=UNDEFINED;
        globalRef.clsTypeIdx=UNDEFINED;
        for(u4 i=0,N=dexGlobal.dexFile->header_->type_ids_size_;i<N&&(globalRef.
                strTypeIdx == UNDEFINED||globalRef.clsTypeIdx==UNDEFINED);++i){
            if(strcmp(getStringFromStringId(dexGlobal.dexFile->string_ids_
                                            [dexGlobal.dexFile->type_ids_[i].descriptor_idx_],dexGlobal.dexFile->begin_),
                      "Ljava/lang/String;")==0){
                globalRef.strTypeIdx=i;
                LOGV("meet str type idx=%d",i);
            }
            if(strcmp(getStringFromStringId(dexGlobal.dexFile->string_ids_
                                            [dexGlobal.dexFile->type_ids_[i].descriptor_idx_],dexGlobal.dexFile->begin_),
                      "Ljava/lang/Class;")==0){
                globalRef.clsTypeIdx=i;
                LOGV("meet class type idx=%d",i);
            }
        }

    }
    dexGlobal.initPoolIfNeeded(runResolver,threadInit,threadDestroy);
    dexGlobal.pool->submit(this);

   // LOGV("Task pended");
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
            int typeIdx=readUnsignedLeb128(ptr,size);
            int address=readUnsignedLeb128(ptr,size);
            tryItem->handlers[i].typeIdx= (u4) typeIdx;
            tryItem->handlers[i].offset= (u4) address;
        }
        if(hasCatchAll){
            tryItem->handlers[hCount].typeIdx=TypeException;
            tryItem->handlers[hCount].offset= (u4) readUnsignedLeb128(ptr, size);
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
        registers[codeItem->registers_size_-paraSize-1]= thizTypeIdx;
        //LOGV("Instance Reg%u init with type=%u",)
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
    if(isLog)LOGV("Vmethod classIdx=%u,vIdx=%x",clsIdx,vIdx);
    const char*clsName =getStringFromStringId(dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->type_ids_[clsIdx].descriptor_idx_],dexGlobal. dexFile->begin_);
    if(clsName[0]!='L'){
        LOGV("Vmethod classIdx=%u,vIdx=%x",clsIdx,vIdx);
        LOGW("Invalid class Foundm name=%s c=%s ",clsName,getStringFromStringId(
                dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->type_ids_[thizTypeIdx].descriptor_idx_],dexGlobal. dexFile->begin_));
        return UNDEFINED;
    }
    char* javaclsName =toJavaClassName(clsName);
    if(isLog)LOGV("Vmethod cls name=%s", javaclsName);
    jstring javaClassName=env->NewStringUTF(javaclsName);
    jobject javaMethod= env->CallStaticObjectMethod(dexGlobal.getToolsClass(), dexGlobal.getGetMethodID(), javaClassName,(jint) vIdx);
    env->DeleteLocalRef(javaClassName);
    delete [] javaclsName;
    if(javaMethod==NULL)
        return UNDEFINED;
    jmethodID methodID=env->FromReflectedMethod(javaMethod);
    if(isLog)LOGV("Vmethod, methodid found=%p",methodID);
    jstring result= (jstring) env->CallStaticObjectMethod(dexGlobal.getToolsClass(), dexGlobal.getConvertMember(), javaMethod);
    env->DeleteLocalRef(javaMethod);
    jboolean isCopy;
    const char* cStr=env->GetStringUTFChars(result, &isCopy);
    char cls[strlen(cStr)+1];
    char *mName,*proto;
    memcpy(cls,cStr,strlen(cStr)+1);
    env->ReleaseStringUTFChars(result,cStr);
    env->DeleteLocalRef(result);
    char* sp=strchr(cls,'|');
    *sp='\0'; mName=sp+1;
    sp=strchr(mName,'|');
    *sp='\0';proto=sp+1;
    //LOGV("Start compare method in dex,cls=%s,name=%s,sig=%s",cls,mName,proto);
    for(u4 i=0;i<dexGlobal.dexFile->header_->method_ids_size_;++i){
        const art::DexFile::MethodId& mid=dexGlobal.dexFile->method_ids_[i];
        const char* thizClass=getStringFromStringId(dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->type_ids_[mid.class_idx_].
                descriptor_idx_],dexGlobal.dexFile->begin_);
        if(strcmp(cls,thizClass)!=0&&strcmp(clsName,thizClass)!=0) continue;
        //LOGV("Compare Name");
        const char* name=getStringFromStringId(dexGlobal.dexFile->string_ids_[mid.name_idx_],dexGlobal.dexFile->begin_);
        if(strcmp(name,mName)!=0) continue;
        std::string protoString;
        getProtoString(dexGlobal.dexFile->proto_ids_[mid.proto_idx_],dexGlobal.dexFile->type_ids_,dexGlobal.dexFile->string_ids_,dexGlobal.dexFile->begin_,protoString);
        if(protoString.compare(proto)==0){
            return i;
        }
    }
    return UNDEFINED;
}
u4 CodeResolver::getFiledFromOffset(u4 clsIdx, u4 fieldOffset) {

    const char*clsName =getStringFromStringId(dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->type_ids_[clsIdx].descriptor_idx_],dexGlobal. dexFile->begin_);
    if(clsName[0]!='L'){
        LOGV("Qfield classIdx=%u,vIdx=%x",clsIdx,fieldOffset);
        LOGW("Invalid class Found f=%s",getStringFromStringId(
                dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->type_ids_[thizTypeIdx].descriptor_idx_],dexGlobal. dexFile->begin_));
    }
    const char*javaClsName =toJavaClassName(clsName);

    jstring javaClassName=env->NewStringUTF(javaClsName);
    jobject javaFiled= env->CallStaticObjectMethod(dexGlobal.getToolsClass(),dexGlobal.getGetFieldID(), javaClassName,(jint) fieldOffset);
    env->DeleteLocalRef(javaClassName);
    delete [] javaClsName;
    if(javaFiled==NULL)
        return UNDEFINED;
    if(isLog)LOGV("Start find field loop ");
    jstring result= (jstring) env->CallStaticObjectMethod(dexGlobal.getToolsClass(), dexGlobal.getConvertMember(), javaFiled);
    env->DeleteLocalRef(javaFiled);
    jboolean isCopy;
    const char* cStr=env->GetStringUTFChars(result, &isCopy);
    char cls[strlen(cStr)+1];
    char *fName,*typeName;
    memcpy(cls,cStr,strlen(cStr)+1);
    env->ReleaseStringUTFChars(result,cStr);
    env->DeleteLocalRef(result);
    char* sp=strchr(cls,'|');
    *sp='\0'; fName=sp+1;
    sp=strchr(fName,'|');
    *sp='\0';typeName=sp+1;
    if(isLog)LOGV("Start find field in dex,%s %s %s",cls,fName,typeName);
    for(u4 i=0;i<dexGlobal.dexFile->header_->file_size_;++i){
        const art::DexFile::FieldId&fid =dexGlobal.dexFile->field_ids_[i];
        const char* thizClass=getStringFromStringId(dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->
                type_ids_[fid.class_idx_].descriptor_idx_],dexGlobal.dexFile->begin_);
        if(strcmp(cls,thizClass)!=0&&strcmp(thizClass,clsName)!=0)
            continue;
        const char* name=getStringFromStringId(dexGlobal.dexFile->string_ids_[fid.name_idx_], dexGlobal.dexFile->begin_);
        if(strcmp(name,fName)!=0) continue;
        const char* type=getStringFromStringId(dexGlobal.dexFile->string_ids_[dexGlobal.dexFile->type_ids_[fid.type_idx_].descriptor_idx_],dexGlobal.dexFile->begin_);
        if(strcmp(type,typeName)!=0)
            LOGW("Type Mismatch field%s.%s:%s,should be type:%s",cls,name,typeName,typeName);
        else{
            return i;
        } //it ok to cast it to u2 as the general method limit.
    }
    return UNDEFINED;
}


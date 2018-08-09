//
// Created by asus on 2016/8/16.
//
#include <cstring>
#include "CodeResolver.h"
#include "dalvik/InlineTable.h"
#include "dalvik/VerfiyError.h"
#include "util/DexUtils.h"
#include "support/OpCodesAndType.h"

const JNINativeMethod getMethods[] = {
        {"getFieldOffset", "(Ljava/lang/reflect/Field;)I",       (void*)getFieldOffset},
        {"getMethodVIdx",  "(Ljava/lang/reflect/Method;)I",      (void *) getMethodVIdx}
};
extern JavaVM* javaVM;
thread_local bool isLog;
static thread_local JNIEnv* env;
static class ref{

public:
    u4 strTypeIdx;
    u4 clsTypeIdx;
    u4 emptyMethodIndex;
    const art::DexFile* curDexFile= nullptr;
} globalRef;


JNIEXPORT jint JNICALL getFieldOffset(JNIEnv *env, jclass thisClass, jobject field) {
    jfieldID fieldID=env->FromReflectedField(field);
    if(env->ExceptionCheck()==JNI_TRUE){
        env->ExceptionClear();
    }
    if(fieldID== nullptr){
        return INT32_MAX;
    }
    if(isDalvik()){
        dalvik::InstField* instField= reinterpret_cast<dalvik::InstField*>(reinterpret_cast<u1*>(fieldID));
        return instField->byteOffset;
    }
    if (dexGlobal.sdkOpt<ART_MARSHMALLOW) {
        ArtFieldLollipop *artField = reinterpret_cast<ArtFieldLollipop *>(reinterpret_cast<u1 *>(fieldID));
        return artField->offset_;
    }
    else {
        ArtField* artField= reinterpret_cast<ArtField*>(reinterpret_cast<u1*>(fieldID));
        return artField->offset_;
    }// optimized field are u2 only
}


JNIEXPORT jint JNICALL  getMethodVIdx(JNIEnv *env,jclass thisClass,jobject method){
    jmethodID methodID =env->FromReflectedMethod(method);
    if(env->ExceptionCheck()==JNI_TRUE){
        env->ExceptionClear();
    }
    if(methodID== nullptr){
        LOGV("%p can't be converted",method);
        return INT32_MAX;
    }
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
        throw std::runtime_error("Register getter native method failed");
    }
    LOGV("Vm thread attached");
}
void CodeResolver::threadDestroy() {
    javaVM->DetachCurrentThread();
    env= nullptr;
    LOGV("Vm thread detached");
}

void CodeResolver::verifyRegCount(u2 insns[], u4 pos) {
    u2 op = insns[pos] & u2(0xff);
    int regCount = 0;
    switch (op) {
        case invokeVirtual:
        case invokeSuper:
        case invokeInterface:
        case invokeDirect:
            regCount--;
        case invokeStatic:
            regCount += insns[pos] >> 12;
            break;
        case invokeVirtualR:
        case invokeSuperR:
        case invokeInterfaceR:
        case invokeDirectR:
            regCount--;
        case invokeStaticR:
            regCount += insns[pos] >> 8;
            break;
        default: {
            return;
        }
    }
    auto &methodId = dexGlobal.dexFile->method_ids_[insns[pos + 1]];
    auto &protoId = dexGlobal.dexFile->
            proto_ids_[methodId.proto_idx_];
    u4 size;
    if (protoId.parameters_off_ == 0) {
        size = 0;
    } else {
        auto typeList = reinterpret_cast<const art::DexFile::TypeList *>(dexGlobal.dexFile->begin_ +
                                                                         protoId.parameters_off_);
        size = typeList->Size();
    }
    if (size != regCount) {
        logMethod(methodId, dexGlobal.dexFile);
        LOGE("Reg count doesn't match,expected=%d,given=%u", regCount, size);
    }
}
void* CodeResolver::runResolver(void *args) {

    CodeResolver*resolver = reinterpret_cast<CodeResolver*>(args);

    const char *clsName = dexGlobal.dexFile->stringFromType(resolver->methodId->class_idx_);

    const char *methodName= dexGlobal.dexFile->stringByIndex(resolver->methodId->name_idx_);
    bool isLog= false;
    /*if (equals("Landroid/support/v7/widget/RecyclerView$LayoutManager;", clsName)&&equals("getItemCount",methodName)) {
         isLog= true;
    } else
        return nullptr;
    sleep(1);*/
    ::isLog = isLog;
    //LOGV("Start Analysis,clsIdx=%u,class=%s,method=%s%s",
     //resolver->methodId->class_idx_, clsName, methodName, &getProtoSig(resolver->methodId->proto_idx_, dexGlobal.dexFile)[0]);
    
    resolver->initTries();
    const art::DexFile::CodeItem* code= resolver->codeItem;

    u4* defaultRegisters=new u4[code->registers_size_];
    resolver->initRegisters(defaultRegisters);
    
    RangeManager rangeManager(resolver,defaultRegisters);
    NodeCache nodeCache;

    u2 insns[code->insns_size_in_code_units_];
    memcpy(insns,code->insns_,2*code->insns_size_in_code_units_);

    u1 preData,opCode,*ins;u4 thisPos=0,lastPos, pos =0;
    //LOGV("Start run resolver size=%u registers=%u ",code->insns_size_in_code_units_,code->registers_size_);
    bool isNpeReturn = false;
    while(true){

        if(pos >= code->insns_size_in_code_units_){
            if(pos!=code->insns_size_in_code_units_){
                LOGW("Pos add wrong,pos=0x%x,cls=%s name=%s",pos,clsName,methodName);
            }
            Next:
            if(nodeCache.isEmpty()){
                goto EndPoint;
            }
            if(isNpeReturn){
                if (isLog)LOGV("Npe Met");
                isNpeReturn= false;
                rangeManager.rollBack(pos);
            }
            JumpNode newNode= nodeCache.popNode();
            if (isLog)LOGV("Try start new range at pos=%u", newNode.ins_pos);
            if(rangeManager.startNewRange(pos,newNode.ins_pos,newNode.registerTypes)){
                pos =newNode.ins_pos;
                if (isLog) rangeManager.printRange();
                continue;
            } else goto Next;
        }
        try {
            rangeManager.checkNextRange(pos);
            rangeManager.checkPos(pos);
        }catch (std::exception& e){
            LOGE("Range check failed,err=%s ,lastOp=0x%x lastpos=0x%x lastpreData=0x%x class=%s m=%s",e.what(),opCode,pos,preData,clsName,methodName);
            throw e;
        }

        if (resolver->tryMap != nullptr ){
            TryItemValue* tryItemValues[2];
            resolver->tryMap->seekTry(pos,tryItemValues);
            for(TryItemValue* try_item:tryItemValues) {
                if(try_item== nullptr) continue;
                if(isLog){
                    LOGV("Into try dispatch,try pos=%u", pos);
                    for(int j=0;j<try_item->handlerSize;++j){
                        LOGV("Handler%d at pos %u",j,try_item->handlers[j].offset);
                    }
                }
                for(int j=0;j<try_item->handlerSize;++j){
                    u4 offset=try_item->handlers[j].offset;
                    if (forkNode(code, nodeCache, rangeManager, pos, offset )) {
                        if(*((u1*)&insns[offset])!=moveExcept){
                            LOGE("Move Exception unexpected ,offset=%u op=%u,pre=%u,handler pos=%d",
                                 offset, *((u1 *) &insns[offset]), (u1) (insns[offset] >> 8), j);
                            throw std::out_of_range("std::out_of_range:moveExcept");
                        }
                        u1 reg= (u1) (insns[offset] >> 8);/*the move exception code*/
                        rangeManager.checkReg(reg);
                        if (isLog)LOGV("Handler forked at pos=%u", offset );
                        nodeCache.top().registerTypes[reg]=try_item->handlers[j].typeIdx;/*pre set exception type*/
                    }else if(isLog){
                        LOGV("Failed to Fork Node at %u in %u",offset,pos);
                    }
                }
            }
        }

        lastPos=thisPos;
        ins= (u1 *) &insns[pos];
        opCode=*ins;
        preData=ins[1];
        thisPos = pos;
        if(isLog){
            LOGV("Op=0x%x pos=%u preData=0x%x",opCode,pos,preData);
        }
        try {
            //Handle Runtime altered code
            if (!isDalvik()) {
                switch (opCode) {
#define CHECK_FIELD_NPE() u1 rOb= preData >> 4;\
                if(rangeManager.regAt(rOb)==TypeZero){\
                    isNpeReturn= true;\
                    goto Next;\
                }
#define SIMPLE_IGET() resolver->alterField(rangeManager, insns, rOb, pos);\
                u1 rA=preData&u1(0xf);\
                rangeManager.regAt(rA)=TypePrimitive;\

#define IGET_CODE(x)CHECK_FIELD_NPE();\
                *ins=x;\
                SIMPLE_IGET(); \
                pos+=2;\
                continue;

                    case igetQ: {
                        IGET_CODE(iget)
                    }
                    case igetWQ: {
                        CHECK_FIELD_NPE();
                        *ins = igetW;
                        resolver->alterField(rangeManager, insns, rOb, pos);
                        u1 rA = preData & u1(0xf);
                        rangeManager.regAt(rA) = TypePrimitive;
                        rangeManager.regAt(rA + 1) = TypePrimitive;
                        pos += 2;
                        continue;
                    }
                    case igetObQ: {
                        CHECK_FIELD_NPE();
                        *ins = igetOb;
                        resolver->alterField(rangeManager, insns, rOb, pos);
                        u1 rA = preData & u1(0xf);
                        rangeManager.regAt(rA) =
                                dexGlobal.dexFile->field_ids_[insns[pos + 1]].type_idx_;
                        pos += 2;
                        continue;
                    }
#define IPUT_CODE(x)CHECK_FIELD_NPE();\
                *ins=x;\
                resolver->alterField(rangeManager, insns, rOb, pos);\
                pos+=2;continue;

                    case iputQ: {
                        IPUT_CODE(iput);
                    }
                    case iputWQ: {
                        IPUT_CODE(iputW);
                    }
                    case iputObQ: {
                        IPUT_CODE(iputOb);
                    }
                    case invokeVirtualQ: {
#define handle_invokeVirtualQ(Op, reg)\
                        if(rangeManager.regAt(reg)==TypeZero){\
                            isNpeReturn= true;\
                            if(isLog)LOGE("Reg %d is null at invoke",reg);\
                            /*NullPointerException;*/\
                            goto Next;\
                        }\
                        try {\
                            u4 methodIdx= resolver->getVMethodFromIndex(rangeManager.regAt(reg), insns[pos + 1]);\
                            if(methodIdx==UNDEFINED){ \
                                const char *className = dexGlobal.dexFile->stringFromType(rangeManager.regAt(reg));\
                                LOGE("Failed invokeVirtualQ,pos=%d,insns[pos]=%d,insns[pos+2]=%d,regNum=%d,regType=%d:%s",pos,insns[pos ],insns[pos + 2],reg,rangeManager.regAt(reg),className);\
                                goto Next;\
                            } \
                            *ins=Op;\
                            resolver->changeIdx(insns, pos, methodIdx);\
                            assert(*ins==Op);\
                            pos+=3;\
                            continue;\
                        }catch(std::exception& e){\
                            const char *className = dexGlobal.dexFile->stringFromType(rangeManager.regAt(reg));\
                            LOGE("Bad invokeVirtualQ,pos=%d,regNum=%d,regType=%d:%s",pos,reg,rangeManager.regAt(reg),className);\
                            throw std::runtime_error(e.what());\
                        }

                        u1 iReg = u1(insns[pos + 2] & 0xf);
                        handle_invokeVirtualQ(invokeVirtual, iReg);
                    }
                    case invokeVirtualRQ: {
                        handle_invokeVirtualQ(invokeVirtualR, insns[pos + 2])
                    }
                    case iputBooleanQ: {
                        IPUT_CODE(iputBoolean);
                    }
                    case iputByteQ: {
                        IPUT_CODE(iputByte);
                    }
                    case iputCharQ: {
                        IPUT_CODE(iputChar);
                    }
                    case iputShortQ: {
                        IPUT_CODE(iputShort);
                    }
                    case igetBooleanQ: {
                        IGET_CODE(igetBoolean)
                    }
                    case igetByteQ: {
                        IGET_CODE(igetByte)
                    }
                    case igetCharQ: {
                        IGET_CODE(igetChar)
                    }
                    case igetShortQ: {
                        IGET_CODE(igetShort)
                    }
                        //Lambda codes may affect some regs,but they will be
                        // reset before quick invoke some just skip them.
                    case invokeLambda:
                    case captureVariable:
                    case createLambda:
                    case liberateVariable:
                    case boxLambda:
                    case unboxLambda: {
                        pos += 2;
                        continue;
                    }
                    default: {
                        break;
                    }
                }
            } else {
                //unfortunately, below 4.1 some opcode are large than 0xff
                switch (opCode) {
                    case OP_RETURN_VOID_BARRIER:
                        ++pos;
                        *ins = returnV;
                        continue;
                    case OP_IGET_VOLATILE: {
#define REWRITE_INTEGER_FIELD(Op) \
                        u2 type= (u2) dexGlobal.dexFile->stringFromType(dexGlobal.dexFile->field_ids_[insns[pos+1]].type_idx_)[0];\
                        switch (type){\
                            case 'B':*ins=Op##Byte;\
                                break;\
                            case 'C':*ins=Op##Char;\
                                break;\
                            case 'F':\
                            case 'I':\
                                *ins=Op;\
                                break;\
                            case 'S':*ins=Op##Short;\
                                break;\
                            case 'Z':*ins=Op##Boolean;\
                                break;\
                            default:{\
                                throw std::runtime_error(formMessage("Unexpected Type For "#Op":","Field idx=",insns[pos+1]," Type=",dexGlobal.dexFile->\
                                    stringFromType(dexGlobal.dexFile->field_ids_[insns[pos+1]].type_idx_)));\
                            }\
                        };\
                        pos+=2;


                        CHECK_FIELD_NPE();
                        REWRITE_INTEGER_FIELD(iget);

                        u1 rA = preData & u1(0xf);
                        rangeManager.regAt(rA) = TypePrimitive;
                        continue;
                    }
                    case OP_IPUT_VOLATILE: {
                        CHECK_FIELD_NPE();
                        REWRITE_INTEGER_FIELD(iput);
                        continue;
                    }
                    case OP_SGET_VOLATILE: {
                        CHECK_FIELD_NPE();
                        REWRITE_INTEGER_FIELD(sget);
                        rangeManager.regAt(preData) = TypePrimitive;
                        continue;
                    }
                    case OP_SPUT_VOLATILE: {
                        REWRITE_INTEGER_FIELD(sput);
                        continue;
                    }
                    case OP_IGET_OBJECT_VOLATILE: {
                        CHECK_FIELD_NPE();
                        *ins = igetOb;
                        u1 rA = preData & u1(0xf);
                        rangeManager.regAt(rA) = dexGlobal.dexFile->
                                field_ids_[insns[pos + 1]].type_idx_;
                        pos += 2;
                        continue;
                    }
                    case OP_IGET_WIDE_VOLATILE: {
                        CHECK_FIELD_NPE();
                        *ins = igetW;
                        u1 rA = preData & u1(0xf);
                        rangeManager.regAt(rA) = TypePrimitive;
                        rangeManager.regAt(rA + 1) = TypePrimitive;
                        pos += 2;
                        continue;
                    }
                    case OP_IPUT_WIDE_VOLATILE: {
                        CHECK_FIELD_NPE();
                        *ins = iputW;
                        pos += 2;
                        continue;
                    }
                    case OP_SGET_WIDE_VOLATILE: {
                        *ins = sgetW;
                        pos += 2;
                        rangeManager.regAt(preData) = TypePrimitive;
                        rangeManager.regAt(preData + 1) = TypePrimitive;
                        continue;
                    }
                    case OP_SPUT_WIDE_VOLATILE: {
                        *ins = sputW;
                        pos += 2;
                        continue;
                    }
                    case OP_IPUT_OBJECT_VOLATILE: {
                        CHECK_FIELD_NPE();
                        *ins = iputOb;
                        pos += 2;
                        continue;
                    }
                    case OP_SGET_OBJECT_VOLATILE: {
                        *ins = igetOb;
                        rangeManager.regAt(preData) = dexGlobal.dexFile->
                                field_ids_[insns[pos + 1]].type_idx_;
                        pos += 2;
                        continue;
                    }
                    case OP_SPUT_OBJECT_VOLATILE: {
                        *ins = sputOb;
                        pos += 2;
                        continue;
                    }
                    case OP_BREAKPOINT: {
                        throw std::runtime_error("OP_BREAKPOINT shouldn't occur in normal code");
                    }
                    case OP_THROW_VERIFICATION_ERROR: {
                        u1 refType = preData >> 6;
                        int ret = 0;
                        std::string errorMsg;
                        switch (refType) {
                            case VERIFY_ERROR_REF_FIELD: {
                                u2 fieldIndex = insns[pos + 1];
                                auto &fieldId = dexGlobal.dexFile->field_ids_[fieldIndex];
                                errorMsg = std::move(formMessage("field=",
                                                                 dexGlobal.dexFile->stringByIndex(
                                                                         fieldId.name_idx_),
                                                                 " of type=",
                                                                 dexGlobal.dexFile->stringFromType(
                                                                         fieldId.type_idx_),
                                                                 " in class=",
                                                                 dexGlobal.dexFile->stringFromType(
                                                                         fieldId.class_idx_)));
                                break;
                            }
                            case VERIFY_ERROR_REF_CLASS: {
                                u4 classType = insns[pos + 1];
                                const char *classTypeStr = dexGlobal.dexFile->stringFromType(
                                        classType);
                                if (insns[pos + 2] == nop) {
                                    if (classTypeStr[1] == '\0')
                                        classType = TypePrimitive;
                                    u4 targetReg = 0;
                                    for (u4 i = code->registers_size_ - 1u; i != -1; --i) {
                                        if (rangeManager.regAt(i) == classType) {
                                            targetReg = i;
                                            break;
                                        }
                                        else if (rangeManager.regAt(i) == TypeZero) {
                                            targetReg = i;
                                        }
                                    }
                                    *ins = fillArray;
                                    ins[1] = 0;
                                    insns[pos + 2] = (u2) targetReg;
                                    errorMsg = std::move(formMessage(
                                            "filled-new-array or filled-new-array/range instruction has class type=",
                                            classTypeStr));
                                    ret = 3;
                                } else {
                                    errorMsg = std::move(formMessage(
                                            "instruction that is one of these five instruction: const-class, new-instance, "
                                                    "check-cast,instance-of or new-array  and has class type=",
                                            classTypeStr));
                                }
                                break;
                            }
                            case VERIFY_ERROR_REF_METHOD: {
                                auto &methodId = dexGlobal.dexFile->method_ids_[insns[pos + 1]];
                                ret = 3;
                                auto &protoId = dexGlobal.dexFile->
                                        proto_ids_[methodId.proto_idx_];
                                u4 size;
                                if (protoId.parameters_off_ == 0) {
                                    size = 0;
                                } else {
                                    auto typeList = reinterpret_cast<const art::DexFile::TypeList *>(
                                            dexGlobal.dexFile->begin_ + protoId.parameters_off_);
                                    size = typeList->Size();
                                }
                                if (size < 5) {
                                    ins[1] = u1(size + 1) << 4;
                                    ins[0] = invokeVirtual;
                                } else {
                                    ins[1] = u1(size + 1);
                                    ins[0] = invokeVirtualR;
                                }
                                if ((insns[pos + 3] & 0xff) == moveResultOb) {
                                    rangeManager.regAt(insns[pos + 3] >> 8) = protoId.return_type_idx_;
                                    ++ret;
                                }
                                errorMsg = std::move(formMessage("method=",
                                                                 dexGlobal.dexFile->stringByIndex(
                                                                         methodId.name_idx_),
                                                                 &getProtoSig(methodId.proto_idx_,
                                                                              dexGlobal.dexFile)[0],
                                                                 " in class=",
                                                                 dexGlobal.dexFile->stringFromType(
                                                                         methodId.class_idx_)));

                            }
                            default: {
                                break;
                            }
                        }
                        LOGW("This app uses api higher than the current with unresolved %s inside method=%s%s of class %s at pos=%d."
                                     "If OK,please fix it later or run it on"
                                     " higher version of system,",
                             &errorMsg[0], methodName, &getProtoSig(resolver->methodId->proto_idx_,
                                                                    dexGlobal.dexFile)[0], clsName,
                             pos);
                        if (ret == 0) {
                            isNpeReturn = true;
                            goto Next;
                        } else {
                            pos += ret;
                            continue;
                        }
                    }
                    case OP_EXECUTE_INLINE: {
                        handleExecuteInline(insns, pos, ins);
                        pos += 3;
                        continue;
                    }
                    case OP_EXECUTE_INLINE_RANGE: {
                        if (rangeManager.regAt(insns[pos + 2]) == TypeZero) {
                            isNpeReturn = true;
                            //NullPointerException;
                            goto Next;
                        }
                        handleExecuteInline(insns, pos, ins);
                        pos += 3;
                        continue;
                    }
                    case OP_IGET_QUICK: {
                        CHECK_FIELD_NPE();
                        SIMPLE_IGET();
                        REWRITE_INTEGER_FIELD(iget);
                        continue;
                    }
                    case OP_IPUT_OBJECT_QUICK: {
                        IPUT_CODE(iputOb);
                    }
                    case OP_IPUT_WIDE_QUICK: {
                        IPUT_CODE(iputW);
                    }
                    case OP_IGET_WIDE_QUICK: {
                        IGET_CODE(igetW);
                    }
#undef IGET_CODE
#undef SIMPLE_IGET
                    case OP_IPUT_QUICK: {
                        CHECK_FIELD_NPE();
                        resolver->alterField(rangeManager, insns, rOb, pos);
                        REWRITE_INTEGER_FIELD(iput);
                        continue;
                    }
#undef REWRITE_INTEGER_FIELD
                    case OP_IGET_OBJECT_QUICK: {
                        CHECK_FIELD_NPE();
                        *ins = igetOb;
                        resolver->alterField(rangeManager, insns, rOb, pos);
                        u1 rA = preData & u1(0xf);
                        rangeManager.regAt(rA) =
                                dexGlobal.dexFile->field_ids_[insns[pos + 1]].type_idx_;
                        pos += 2;
                        continue;
                    }
#undef CHECK_FIELD_NPE
                    case OP_INVOKE_OBJECT_INIT_RANGE: {
                        if (preData == 0) {
                            *ins = invokeDirectR;
                        } else if (preData == 1) {
                            insns[pos] = invokeDirect | 1 << 12;
                        } else
                            throw std::runtime_error("OP_INVOKE_OBJECT_INIT_RANGE with illegal op");
                        pos += 3;
                        continue;
                    }
                    case OP_INVOKE_VIRTUAL_QUICK:
                    case OP_INVOKE_SUPER_QUICK: {
                        u1 iReg = u1(insns[pos + 2] & 0xf);
                        handle_invokeVirtualQ(opCode - OP_INVOKE_VIRTUAL_QUICK + invokeVirtual,
                                              iReg);
                    }
                    case OP_INVOKE_VIRTUAL_QUICK_RANGE:
                    case OP_INVOKE_SUPER_QUICK_RANGE: {
                        handle_invokeVirtualQ(
                                opCode - OP_INVOKE_VIRTUAL_QUICK_RANGE + invokeVirtualR,
                                insns[pos + 2]);
                    }
                    default: {
                        break;
                    }
                }
            }
            assert(opCode<0xe3);
            //Handle Normal Code
            switch (opCode){
                case move:
                case moveOb:{
                    u1 rA= preData & (u1)0xf;
                    rangeManager.regAt(rA)=rangeManager.regAt(preData >> 4);
                    if(isLog){
                        LOGV("Reg %u Updated with %d from reg %u",rA,rangeManager.regAt(rA),preData>>4);
                    }
                    pos++;
                    break;
                }
                case moveW:{
                    u1 rA= preData & (u1)0xf;
                    rangeManager.regAt(rA) = TypePrimitive;
                    rangeManager.regAt(rA + 1) = TypePrimitiveExtend;
                    pos++;break;
                }
                case moveF16W:
                    rangeManager.regAt(preData + 1) = TypePrimitiveExtend;
                case moveF16:
                case moveObF16:
                    rangeManager.regAt(preData)=rangeManager.regAt(insns[pos + 1]);
                    if(isLog){
                        LOGV("Reg %u Updated with %d from reg %u",preData,rangeManager.regAt(preData),insns[pos+1]);
                    }
                    pos +=2;break;
                case move16W:
                    rangeManager.regAt(insns[pos + 1] + 1) = TypePrimitiveExtend;
                case move16:
                case moveOb16:
                    rangeManager.regAt(insns[pos + 1])=rangeManager.regAt(insns[pos + 2]);
                    if(isLog){
                        LOGV("Reg %u Updated with %d from reg %u",insns[pos + 2],rangeManager.regAt(insns[pos + 2]),insns[pos+2]);
                    }
                    pos +=3;break;
                case moveResultW:
                    rangeManager.regAt(preData + 1) = TypePrimitiveExtend;
                case moveResult:
                    rangeManager.regAt(preData)=TypePrimitive;
                    ++pos;
                    if(isLog){
                        LOGV("Reg %u Updated with %d",preData,TypePrimitive);
                    }
                    break;
                case moveResultOb:{
                    ins=(u1 *) &insns[lastPos];
                    opCode= *ins;
                    switch(opCode){
                        case fillArray:
                        case fillArrayR:
                            rangeManager.regAt(preData) = insns[lastPos +
                                                                    1];//should a type judge be applied here?
                            break;
                        default:{
                            const art::DexFile::MethodId& methodId=
                                    dexGlobal.dexFile->method_ids_[insns[lastPos + 1]];
                            //if(isLog)logMethod(methodId, dexGlobal.dexFile);
                            rangeManager.regAt(preData)= dexGlobal.dexFile->
                                    proto_ids_[methodId.proto_idx_].return_type_idx_;
                            if(isLog)LOGV("%s Meet move result, last op=%d,reg %d updated with %d",clsName,opCode,preData,rangeManager.regAt(preData));
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
                case const32: {
                    u4 value = insns[pos + 1] | (insns[pos + 2] << 16);
                    if (value == 0)
                        rangeManager.regAt(preData) = TypeZero;
                    else rangeManager.regAt(preData) = TypePrimitive;
                    pos += 3;
                    break;
                }
                case const16: {
                    u2 value = insns[pos + 1];
                    if (value == 0)
                        rangeManager.regAt(preData) = TypeZero;
                    else rangeManager.regAt(preData) = TypePrimitive;
                    pos += 2;
                    break;
                }
                case const16H: {
                    pos += 2;
                    rangeManager.regAt(preData) = TypePrimitive;
                    break;
                }
                case const4: {
                    ++pos;
                    u1 value = preData >> 4;
                    if (value == 0)
                        rangeManager.regAt(preData & 0xfu) = TypeZero;
                    else rangeManager.regAt(preData & 0xfu) = TypePrimitive;
                    break;
                }
                case arrayLen:++pos;
                    rangeManager.regAt(u1(preData & 0xf))=TypePrimitive;
                    break;
                case instanceOf:
                    rangeManager.regAt(u1(preData & 0xf))=TypePrimitive;
                    rangeManager.regAt(u1(preData >>4))=insns[pos+1];//preset if necessary

                    pos+=2;
                    break;
                case const64W:
                    pos +=2;
                case const32W:++pos;
                case const16W:
                case const16HW:
                    pos +=2;
                    rangeManager.regAt(preData)=TypePrimitive;
                    rangeManager.regAt(preData + 1u) = TypePrimitiveExtend;
                    break;
                case constStrJ:++pos;
                case constStr:pos+=2;
                    if (globalRef.strTypeIdx == UNDEFINED)
                        throw std::runtime_error("String Type not initiated");
                    rangeManager.regAt(preData)=globalRef.strTypeIdx;
                    break;
                case constClass:
                    if (globalRef.clsTypeIdx == UNDEFINED)
                        throw std::runtime_error("String Type not initiated");
                    rangeManager.regAt(preData)=globalRef.clsTypeIdx;pos+=2;
                    break;
                case checkCast:
                case newInstance:
                    rangeManager.regAt(preData)=insns[++pos];
                    ++pos;
                    break;
                case newArray:
                    if(isLog)LOGV("Meet new Array reg%u updated with %u",u1(preData & 0xf),insns[pos+1]);
                    rangeManager.regAt(u1(preData & 0xf))=insns[pos+1];
                    pos+=2;
                    break;
                case fillArray:
                case fillArrayR:
                case fillArrayData:pos+=3;
                    break;
                case goto8:{
                    u4 newAddress=(u4)(pos+(int8_t) preData);
                    if(rangeManager.rangeJump( pos, newAddress)) {
                        // LOGV("%s goto %u",clsName,newAddress);
                        pos = newAddress;
                        break;
                    }else goto Next;
                }

                case goto16:{
                    u4 newAddress=(u4)(pos +(int16_t) insns[pos+1]);
                    if(rangeManager.rangeJump( ++pos, newAddress)){
                        pos=newAddress;
                        break;
                    } else goto Next;
                }
                case goto32:{
                    u4 address=u4(pos+*((int32_t*)&insns[pos+1]));
                    if(rangeManager.rangeJump( (pos+= 2), address)){
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
                        forkNode(code, nodeCache, rangeManager, pos + 2, pos + targets[i]);
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
                        forkNode(code,  nodeCache, rangeManager, pos + 2, pos + targets[i]);
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
                    forkNode(code, nodeCache, rangeManager, pos + 1,
                             pos + (int16_t)/*avoid auto expand to u4*/insns[pos + 1]);
                    pos+=2;
                    break;
                case agetW:
                case sgetW:
                    rangeManager.regAt(preData + 1) = TypePrimitiveExtend;
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
                    rangeManager.regAt(preData) = TypePrimitive;/*ignore primitive array as this operation has no effect on right code*/
                    pos+=2;break;
                }
                case igetW:
                    rangeManager.regAt((preData & 0xfu) + 1) = TypePrimitiveExtend;
                case iget:
                case igetBoolean:
                case igetByte:
                case igetChar:
                case igetShort:{
                    u1 rOb= preData >> 4;
                    if (rangeManager.regAt(rOb) == TypeZero) {
                        isNpeReturn= true;
                        goto Next;
                    }
                    rangeManager.regAt(preData&0xfu)=TypePrimitive;
                    pos+=2;
                    break;
                }
                case agetOb:{
                    u1 typeReg = (u1) (insns[pos + 1] & 0xff);
                    u4 arrayType = rangeManager.regAt(typeReg);
                    if (arrayType == TypeZero) {
                        isNpeReturn = true;
                        goto Next;
                    }
                    u4 type = UNDEFINED;
                    const char* typeName= dexGlobal.dexFile->stringFromType(arrayType);
                    if(typeName[0]=='['){
                        if(strcmp(typeName+1,"Ljava/lang/String;")==0){
                            type=globalRef.strTypeIdx;
                        } else if(strcmp(typeName+1,"Ljava/lang/Class;")==0){
                            type=globalRef.clsTypeIdx;
                        } else{
                            auto dexFile = dexGlobal.dexFile;
                            type = binarySearchType(typeName + 1, dexFile);
                            if (type == UNDEFINED) {
                                LOGW("Can't find array component type with name %s by binary search,loop find",
                                     typeName + 1);
                                // for unordered dexFile;
                                for (u4 i = 0, N = dexFile->header_->type_ids_size_; i < N; ++i) {
                                    if (strcmp(dexGlobal.dexFile->stringFromType(i),
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
                        //It may be the case that it's a temporary array of multi-dimension array type.
                        LOGW("can't find class type for array type=%s", typeName);
                    }
                    rangeManager.regAt(preData)=type;
                    pos+=2;break;
                }
                case igetOb:{
                    u1 rOb= preData >> 4;
                    if (rangeManager.regAt(rOb) == TypeZero) {
                        isNpeReturn= true;
                        goto Next;
                    }
                    uint16_t typeIdx = dexGlobal.dexFile->
                            field_ids_[insns[pos+1]].type_idx_;
                    rangeManager.regAt(preData & 0xfu)= typeIdx;
                    pos+=2;break;
                }
                case sgetOb:{
                    u4 type=dexGlobal.dexFile->
                            field_ids_[insns[pos+1]].type_idx_;
                    rangeManager.regAt(preData)=type ;
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
                    if (rangeManager.regAt(rOb) == TypeZero) {
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
                    if (rangeManager.regAt(iReg) == TypeZero) {
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
                    if (rangeManager.regAt(insns[pos + 2]) == TypeZero) {
                        isNpeReturn= true;
                        //NullPointerException;
                        goto Next;
                    }
                    pos+=3;
                    break;
                }
                case invokeStaticR:{pos+=3;break;}

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
                        rangeManager.regAt(preData)=TypePrimitive;
                        pos+=2;
                        break;
                    } else if(opCode>=0xd0&&opCode<=0xd7){
                        rangeManager.regAt(u1(preData&0xf))=TypePrimitive;
                        pos+=2;
                        break;
                    }
                    ++pos;
                    break;
                }
            }
        } catch (std::exception &e) {
            LOGE("Meet exception %s,Op=0x%x pos=0x%x,clsIdx=%u,cls=%s,m=%s", e.what(),
                 opCode, pos, resolver->methodId->class_idx_, clsName, methodName);
            goto EndPoint;
        }
    }
    EndPoint:
        if(isLog)LOGV("Goto end point");
    //In some cases,some bad codes that call a instance method without instance
    //can cause the codeFix to fail,some some logs and some replacement should be generate to fix these codes
    //actually these codes can run without fix at all as they must throw a NullPointerException when reach.
    //But as they may cause dex2smali tools to throw an exception, so some fixes is required.
    //For more informations, see below.
    if (checkAndReplaceOpCodes(insns, code->insns_size_in_code_units_)) {
        LOGE("CodeFix is not over yet in method=%s%s;in class %s that is at No.%u classDef;",
             methodName, &getProtoSig(resolver->methodId->proto_idx_, dexGlobal.dexFile)[0],
             clsName,
             searchClassPos(clsName));
        //lastRange->printRange();
    }
    int fd=open(dexGlobal.dexFileName,O_WRONLY);
    pwrite(fd, insns, (size_t) code->insns_size_in_code_units_ << 1, (off_t) resolver->fileOffset);
    close(fd);
    if(isLog)LOGV("Write insns Over");
    delete resolver;
    return nullptr;
}

bool CodeResolver::checkAndReplaceOpCodes(u2 *insns, u4 insns_size) {
    u4 i, opCount;
    bool ret = false;
    for (i = opCount = 0; i < insns_size; ++opCount) {
        u1 *opPtr = (u1 *) (insns + i);
        u1 opCode = *opPtr;
        //verifyRegCount(insns,i);
        if (isDalvik()) {
            switch (opCode) {
                case OP_RETURN_VOID_BARRIER:
                    *opPtr = returnV;
                    i += 1;
                    continue;
                case OP_INVOKE_OBJECT_INIT_RANGE:
                    if ((insns[i] >> 8) == 0) {
                        *opPtr = invokeDirectR;
                    } else {
                        insns[i] = invokeDirect | 1 << 12;
                    }
                    i += 3;
                    continue;
                case OP_THROW_VERIFICATION_ERROR: {
                    int refType = insns[i] >> 14;
                    std::string errMsg;
                    switch (refType) {
                        case VERIFY_ERROR_REF_METHOD: {
                            auto &methodId = dexGlobal.dexFile->method_ids_[insns[i + 1]];
                            errMsg = formMessage("method ",
                                                 dexGlobal.dexFile->stringByIndex(
                                                         methodId.name_idx_),
                                                 &getProtoSig(methodId.proto_idx_,
                                                              dexGlobal.dexFile)[0], " in class:",
                                                 dexGlobal.dexFile->stringFromType(
                                                         methodId.class_idx_));

                            break;
                        }
                        case VERIFY_ERROR_REF_CLASS:
                            errMsg = formMessage("class ",
                                                 dexGlobal.dexFile->stringFromType(
                                                         insns[i + 1]));
                            break;
                        case VERIFY_ERROR_REF_FIELD: {
                            auto &fieldId = dexGlobal.dexFile->field_ids_[insns[i + 1]];
                            errMsg = formMessage("field ",
                                                 dexGlobal.dexFile->stringByIndex(
                                                         fieldId.name_idx_), " of type:",
                                                 dexGlobal.dexFile->stringFromType(
                                                         fieldId.type_idx_), " in class:",
                                                 dexGlobal.dexFile->stringFromType(
                                                         fieldId.class_idx_));
                            break;
                        }
                        default: {
                            break;
                        }

                    }
                    insns[i] = 0;
                    insns[i + 1] = 0;
                    i += 2;
                    if (!ret) ret = true;
                    LOGE("Still meet unhandled OP_THROW_VERIFICATION_ERROR in the final process at"
                                 " #%d instruction,with unresolved %s", opCount, &errMsg[0]);
                    continue;
                }

                case OP_EXECUTE_INLINE:
                case OP_EXECUTE_INLINE_RANGE:
                    handleExecuteInline(insns, i, opPtr);
                    i += 3;
                    continue;
#define REPLACE_RAW(cur, orig)\
                case cur:*opPtr=orig;

#define REPLACE_SIMPLE(cur, orig, offset)\
                    REPLACE_RAW(cur,orig) if(!ret){ret=true;}\
                    LOGE("Unresolved OpCode replaced from "#cur" 0x%x to "#orig\
                          " 0x%x with index=%u at the #%u instruction at pos=%u",cur,orig,insns[i+1],opCount,i);\
                           i+=offset;

#define REPLACE_FIELD(cur, orig)insns[i+1]=0; REPLACE_SIMPLE(cur,orig,2) continue;

#define REPLACE_VOLATILE(cur, orig) REPLACE_RAW(cur,orig) i+=2;continue;

                REPLACE_VOLATILE(OP_SGET_VOLATILE, sget)
                REPLACE_VOLATILE(OP_SGET_OBJECT_VOLATILE, sgetOb)
                REPLACE_VOLATILE(OP_SGET_WIDE_VOLATILE, sgetW)
                REPLACE_VOLATILE(OP_SPUT_OBJECT_VOLATILE, sputOb)
                REPLACE_VOLATILE(OP_SPUT_VOLATILE, sput)
                REPLACE_VOLATILE(OP_SPUT_WIDE_VOLATILE, sputW)
                REPLACE_FIELD(OP_IGET_OBJECT_QUICK, igetOb)
                REPLACE_FIELD(OP_IGET_OBJECT_VOLATILE, igetOb)
                REPLACE_FIELD(OP_IGET_QUICK, iget)
                REPLACE_FIELD(OP_IGET_VOLATILE, iget)
                REPLACE_FIELD(OP_IGET_WIDE_QUICK, igetW)
                REPLACE_FIELD(OP_IGET_WIDE_VOLATILE, igetW)
                REPLACE_FIELD(OP_IPUT_OBJECT_QUICK, iputOb)
                REPLACE_FIELD(OP_IPUT_OBJECT_VOLATILE, iputOb)
                REPLACE_FIELD(OP_IPUT_QUICK, iput)
                REPLACE_FIELD(OP_IPUT_VOLATILE, iput)
                REPLACE_FIELD(OP_IPUT_WIDE_QUICK, iputW)
                REPLACE_FIELD(OP_IPUT_WIDE_VOLATILE, iputW)
#define  ReplaceWithEmptyMethod\
                    insns[i-1]=0;\
                    insns[i-2]= (u2) globalRef.emptyMethodIndex;\
                    continue;

                REPLACE_SIMPLE(OP_INVOKE_VIRTUAL_QUICK, invokeVirtual, 3)
                    insns[i - 3] = *opPtr | u2(1 << 12);
                    ReplaceWithEmptyMethod;

                REPLACE_SIMPLE(OP_INVOKE_SUPER_QUICK, invokeSuper, 3)
                    insns[i - 3] = *opPtr | u2(1 << 12);
                    ReplaceWithEmptyMethod;

                REPLACE_SIMPLE(OP_INVOKE_VIRTUAL_QUICK_RANGE, invokeVirtualR, 3)
                    insns[i - 3] = *opPtr | u2(1 << 8);
                    ReplaceWithEmptyMethod;

                REPLACE_SIMPLE(OP_INVOKE_SUPER_QUICK_RANGE, invokeSuperR, 3)
                    insns[i - 3] = *opPtr | u2(1 << 8);
                    ReplaceWithEmptyMethod;
#undef REPLACE_RAW
#undef REPLACE_FIELD
#undef REPLACE_SIMPLE
                default: {
                    break;
                };
            }
        } else {
            switch (opCode) {
                case returnVNo:
                    *opPtr = returnV;
                    i += 1;
                    continue;
#define SIMPLE_REPLACE(opCode, offset)\
                case opCode##Q: if(!ret){ret=true;}*opPtr=opCode; \
                                    LOGE("Unresolved OpCode replaced from "#opCode"Q 0x%x to "#opCode\
                                    " 0x%x with index=%u at the #%u instruction at pos=%u",opCode##Q,opCode,insns[i+1],opCount,i);\
                                    i+=offset;

#define SIMPLE_REPLACE_FIELD(opCode) insns[i+1]=0; SIMPLE_REPLACE(opCode,2)\
                    continue;

                SIMPLE_REPLACE_FIELD(iget)

                SIMPLE_REPLACE_FIELD(igetW)
                SIMPLE_REPLACE_FIELD(igetOb)
                SIMPLE_REPLACE_FIELD(iput)
                SIMPLE_REPLACE_FIELD(iputW)
                SIMPLE_REPLACE_FIELD(iputOb)

                SIMPLE_REPLACE(invokeVirtual, 3)
                    insns[i - 3] = *opPtr | u2(1 << 12);
                    ReplaceWithEmptyMethod;

                SIMPLE_REPLACE(invokeVirtualR, 3)
                    insns[i - 3] = *opPtr | u2(1 << 8);
                    ReplaceWithEmptyMethod;

                SIMPLE_REPLACE_FIELD(iputBoolean)
                SIMPLE_REPLACE_FIELD(iputByte)
                SIMPLE_REPLACE_FIELD(iputChar)
                SIMPLE_REPLACE_FIELD(iputShort)

                SIMPLE_REPLACE_FIELD(igetBoolean)
                SIMPLE_REPLACE_FIELD(igetByte)
                SIMPLE_REPLACE_FIELD(igetChar)
                SIMPLE_REPLACE_FIELD(igetShort)

#undef SIMPLE_REPLACE
#undef SIMPLE_REPLACE_FIELD
                case 0xf3:
                case 0xf5:
                case 0xf6:
                case 0xf7:
                case 0xf8:
                case 0xf9: {
                    i += 2;
                    continue;
                }
                default: {
                    break;
                }
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
                    i += tableSize;
                    break;
                }
                default: {
                    if (tag != 0)
                        LOGW("Unrecognized 0 tag %x, skip it", tag);
                    ++i;
                    break;
                }

            }
        }
    }
    return ret;
}

void CodeResolver::alterField(RangeManager &rangeManager,
                              u2 *insns, u1 rOb, u4 pos) {
    //if(isLog)LOGV("Start get field offset rOb=%u,typeIdx=%u",rOb,rangeManager.getRegs()[rOb]);
    u4 fieldIdx =getFiledFromOffset(rangeManager.regAt(rOb), insns[pos + 1]);
    //if(isLog)LOGV("Field index Gotten %d",fieldIdx);
    changeIdx(insns, pos, fieldIdx);
}

void CodeResolver::changeIdx(u2 *insns, u4 pos, u4 Idx) const {
    if(Idx == UNDEFINED){
        LOGW("Unable to find index at pos%u;class=%s,method=%s%s", pos,
             dexGlobal.dexFile->stringFromType(methodId->class_idx_),
             dexGlobal.dexFile->stringByIndex(methodId->name_idx_),
             &getProtoSig(methodId->proto_idx_, dexGlobal.dexFile)[0]);
    }else insns[pos+1]= (u2) Idx;
}

bool CodeResolver::forkNode(const art::DexFile::CodeItem *code, NodeCache &nodeCache,
                           RangeManager &rangeManager, u4 lastPos, u4 newPos) {
    if(newPos>code->insns_size_in_code_units_){
        throw std::out_of_range(formMessage("Invalid new pos=", newPos));
    }
    if (rangeManager.checkRange(newPos, lastPos)) {
        //LOGV("New Node Forked from %u to %u",lastPos,newPos);
        u4* regs=new u4[code->registers_size_];
        memcpy(regs, &rangeManager.regAt(0), code->registers_size_ << 2);
        nodeCache.addNode(newPos,regs);
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
        globalRef.strTypeIdx = binarySearchType("Ljava/lang/String;", globalRef.curDexFile);
        if (globalRef.strTypeIdx == UNDEFINED) {
            for (u4 i = 0, N = dexGlobal.dexFile->header_->type_ids_size_; i < N && (globalRef.
                    strTypeIdx == UNDEFINED || globalRef.clsTypeIdx == UNDEFINED); ++i) {
                auto className = dexGlobal.dexFile->stringFromType(i);
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
                if (strcmp(dexGlobal.dexFile->stringFromType(i),
                           "Ljava/lang/Class;") == 0) {
                    globalRef.clsTypeIdx = i;

                    break;
                }
            }
        }
        for (u4 i = 0, N = dexGlobal.dexFile->header_->method_ids_size_; i < N; ++i) {
            auto &methodId = dexGlobal.dexFile->method_ids_[i];
            auto &protoId = dexGlobal.dexFile->proto_ids_[methodId.proto_idx_];
            const char *methodName = dexGlobal.dexFile->stringByIndex(methodId.name_idx_);
            if (protoId.parameters_off_ == 0 && methodName[0] != '<') { ;
                LOGV("Meet empty method,id=%d method=%s%s in class=%s", i, methodName,
                     &getProtoSig(methodId.proto_idx_, dexGlobal.dexFile)[0],
                     dexGlobal.dexFile->stringFromType(methodId.class_idx_));
                globalRef.emptyMethodIndex = i;
                break;
            }
        }
        auto& fieldId=dexGlobal.dexFile->field_ids_[0];
        LOGV("Meet empty field,id=0 field=%s:%s in class=%s",
             dexGlobal.dexFile->stringByIndex(fieldId.name_idx_)
        , dexGlobal.dexFile->stringFromType(fieldId.type_idx_),
             dexGlobal.dexFile->stringFromType(fieldId.class_idx_));
        LOGV("Meet str type idx=%d", globalRef.strTypeIdx);
        LOGV("Meet class type idx=%d", globalRef.clsTypeIdx);

    }
    dexGlobal.initPoolIfNeeded(runResolver,threadInit,threadDestroy);
    dexGlobal.pool->submit(this);

    return true;
}

void CodeResolver::initRegisters(u4* registers) {
    u4 paraSize = 0;
    if (protoList != nullptr) {
        for (u4 i = protoList->Size() - 1; i != -1; --i) {
            u4 typeIdx = protoList->GetTypeItem(i).type_idx_;
            char type = dexGlobal.dexFile->stringFromType(typeIdx)[0];
            switch (type) {
                case '\0':
                    LOGE("Unexpected type zero in paraList");
                    continue;
                case '[':
                case 'L':
                    ++paraSize;
                    assert(codeItem->registers_size_ - paraSize<codeItem->registers_size_);
                    registers[codeItem->registers_size_ - paraSize] = typeIdx;
                    continue;
                case 'J':
                case 'D':
                    ++paraSize;
                    registers[codeItem->registers_size_ - paraSize] = TypePrimitive;
                default: {
                    ++paraSize;
                    assert(codeItem->registers_size_ - paraSize<codeItem->registers_size_);
                    registers[codeItem->registers_size_ - paraSize] = TypePrimitive;
                    break;
                }
            }
        }
        assert(paraSize <= codeItem->registers_size_);
    }

    //LOGV("Into init registers,registerSize=%d,paraSize=%d,isInstance=%d",codeItem->registers_size_,paraSize,isInstance);

    int paraStart=codeItem->registers_size_-paraSize;

    u4 i=0;
    for(;i<paraStart;++i){
        registers[i]=UNDEFINED;
    }

    if(isInstance) {
        registers[codeItem->registers_size_ - paraSize - 1] = methodId->class_idx_;
    }
}

void CodeResolver::handleExecuteInline(u2 insns[], u4 pos, u1 *ins) {
    u2 idx = insns[pos + 1];
    if (idx >= InlineOpsTableSize) {
        throw std::runtime_error(formMessage("ExecuteIdx out of range:", idx));
    }
    if (idx >= InlineVirtualStart && idx <= InlineVirtualEnd) {
        *ins = *ins == OP_EXECUTE_INLINE ? invokeVirtual : invokeVirtualR;
    } else *ins = *ins == OP_EXECUTE_INLINE ? invokeStatic : invokeStaticR;
    InlineMethod &inlineMethod = InlineOpsTable[idx];
    if (inlineMethod.methodIdx == UNDEFINED) {
        inlineMethod.methodIdx = binarySearchMethod(
                inlineMethod.classDescriptor, inlineMethod.methodName,
                inlineMethod.retType, inlineMethod.parSig);
    }
    insns[pos + 1] = (u2) inlineMethod.methodIdx;
}
u4 CodeResolver::getVMethodFromIndex(u4 classIdx, u4 vIdx) {
    if (classIdx == TypeException) {
        LOGW("Unexpected type TypeException,as catch all has no type specified");
        return UNDEFINED;
    }
    if (classIdx == UNDEFINED) {
        LOGW("Class Undef");
        return UNDEFINED;
    }
    return dexGlobal.dexSeeker->getMethodByIndex(env,classIdx,vIdx);
    //if(isLog)LOGV("Vmethod classIdx=%u,vIdx=%x",classIdx,vIdx);
    const char *className = dexGlobal.dexFile->stringFromType(classIdx);
    if (className[0] != 'L' && className[0] !=
                               '[') {//Array type is namely sub-type of object,so inherit all the virtual methods of object
        LOGE("Invalid class Foundm name=%s c=%s ", className,
             dexGlobal.dexFile->stringFromType(methodId->class_idx_));
        return UNDEFINED;
    }
    JavaString ClassName(toJavaClassName(className));
    jstring javaClassName = env->NewString(&ClassName[0],ClassName.Count());
    if (isLog)LOGV("Vmethod cls name=%s", &ClassName.toUtf8()[0]);
    if (javaClassName == nullptr) {
        LOGE("Failed to get java class name");
        return UNDEFINED;
    }
    jbyteArray result = (jbyteArray) env->CallStaticObjectMethod(dexGlobal.getToolsClass(),
                                                                 dexGlobal.getGetMethodID(),
                                                                 javaClassName, (jint) vIdx);
    env->DeleteLocalRef(javaClassName);
    if (result == NULL) {
        return UNDEFINED;
    }
    jboolean isCopy;
    char *cStr = (char *) env->GetPrimitiveArrayCritical(result, &isCopy);
    if(env->ExceptionCheck()==JNI_TRUE){
        env->ExceptionClear();
    }
    char cls[strlen(cStr) + 1];
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
    if ((sp = strchr(proto, '|')) != nullptr) {
        *sp = '\0';
    }
    if (isLog)
        LOGV("Start compare method in dex,cls=%s,name=%s,sig=(%s)%s", cls, mName, proto, retType);
    u4 ret = binarySearchMethod(cls, mName, retType, proto);
    if (ret != UNDEFINED) return ret;
    while (sp != nullptr) {
        sp= strchr(sp + 1, '|');
        if (sp != nullptr) *sp = '\0';
        ret = binarySearchMethod(className, mName, retType, proto);
        if (ret != UNDEFINED) return ret;
    }

    *(mName - 1) = '|';
    *(retType - 1) = '|';
    *(proto - 1) = '|';
    /*LOGE("Can't find method id for %s in ordered mode,start full glance", cls);
    *(mName - 1) = '\0';
    *(retType - 1) = '\0';
    *(proto - 1) = '\0';

    for (u4 i = 0; i < dexGlobal.dexFile->header_->method_ids_size_; ++i) {
          const art::DexFile::MethodId& mid=dexGlobal.dexFile->method_ids_[i];
          const char* thizClass=dexGlobal.dexFile->stringFromType(mid.class_idx_);
          if(strcmp(cls,thizClass)!=0&& strcmp(className, thizClass) != 0) continue;
          const char* name=dexGlobal.dexFile->stringByIndex(mid.name_idx_);
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
    throw std::runtime_error(formMessage("Can't find method id for", cls));*/
    return UNDEFINED;
}

u4 CodeResolver::getFiledFromOffset(u4 classIdx, u4 fieldOffset) {
    const char *className = dexGlobal.dexFile->stringFromType(classIdx);
    if (className[0] != 'L') {//only object type has field.
        LOGE("Qfield Invalid class Found f=%s,classIdx=%u,vIdx=%x",
             dexGlobal.dexFile->stringFromType(methodId->class_idx_), classIdx,
             fieldOffset);
    }
    return dexGlobal.dexSeeker->getFieldIdFromOffset(env,classIdx,fieldOffset);
    JavaString ClassName(toJavaClassName(className));
    jstring javaClassName = env->NewString(&ClassName[0],ClassName.Count());
    jbyteArray result = (jbyteArray) env->CallStaticObjectMethod(dexGlobal.getToolsClass(),
                                                                 dexGlobal.getGetFieldID(),
                                                                 javaClassName, (jint) fieldOffset);
    env->DeleteLocalRef(javaClassName);
    if (result == NULL)
        return UNDEFINED;

    jboolean isCopy;
    char *cStr = (char *) env->GetPrimitiveArrayCritical(result, &isCopy);
    if(env->ExceptionCheck()==JNI_TRUE){
        env->ExceptionClear();
    }
    //LOGV("Start find field with result=%s",cStr);
    char cls[strlen(cStr) + 1];
    char *fName,*typeName;
    memcpy(cls,cStr,strlen(cStr)+1);
    env->ReleasePrimitiveArrayCritical(result, cStr, 0);
    env->DeleteLocalRef(result);
    char* sp=strchr(cls,'|');
    *sp='\0'; fName=sp+1;
    sp=strchr(fName,'|');
    *sp='\0';typeName=sp+1;
    if ((sp = strchr(typeName, '|')) != nullptr) {
        *sp = '\0';
    }
    //LOGV("Start find field in dex,%s %s %s",className,fName,typeName);
    u4 ret = binarySearchField(cls, fName, typeName);
    if (ret != UNDEFINED) return ret;
    while (sp != nullptr) {
        sp = strchr(sp+1, '|');
        if (sp != nullptr) *sp = '\0';
        //LOGV("search field in dex,%s %s %s",className,fName,typeName);
        ret = binarySearchField(className, fName, typeName);
        if (ret != UNDEFINED) return ret;
    }


    *(typeName - 1) = '|';
    *(fName - 1) = '|';
    /*LOGE("Can't find field id for %s by binary search start full glance", cls);
    *(typeName - 1) = '\0';
    *(fName - 1) = '\0';

    for (u4 i = 0; i < dexGlobal.dexFile->header_->field_ids_size_; ++i) {
        const art::DexFile::FieldId &fid = dexGlobal.dexFile->field_ids_[i];
        const char *thizClass = dexGlobal.dexFile->stringFromType(fid.class_idx_);
        if (strcmp(cls, thizClass) != 0 && strcmp(thizClass, className) != 0)
            continue;
        const char *name = dexGlobal.dexFile->stringByIndex(fid.name_idx_);
        if (strcmp(name, fName) != 0) continue;
        const char *type = dexGlobal.dexFile->stringFromType(fid.type_idx_);
        if (strcmp(type, typeName) != 0)
            LOGW("Type Mismatch field%s.%s:%s,should be type:%s", cls, name, typeName, typeName);
        else {
            return i;
        }
    }
    *(typeName - 1) = '|';
    *(fName - 1) = '|';*/
    throw std::runtime_error(formMessage("Can't find field id for", cls));
    return UNDEFINED;
}

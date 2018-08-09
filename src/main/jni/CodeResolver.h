//
// Created by asus on 2016/8/16.
//

#ifndef HOOKMANAGER_CODERESOLVER_H
#define HOOKMANAGER_CODERESOLVER_H

#include <pthread.h>
#include <unordered_map>
#include "jni.h"
#include "support/art-member.h"
#include "DexCommons.h"
#include "dalvik/Object.h"
#include "support/dex_file.h"
#include "support/OpCodesAndType.h"
#include <stack>
typedef std::unordered_map<jfieldID ,u4> FieldMap;
typedef std::unordered_map<jmethodID ,u4> MethodMap;
#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT jint JNICALL getFieldOffset(JNIEnv *env,jclass thisClass,jobject field);
JNIEXPORT jint JNICALL  getMethodVIdx(JNIEnv *env,jclass thisClass,jobject method);
#ifdef __cplusplus
}

class CodeResolver{
private:

    class JumpNode {
    public:
        u4 ins_pos=UNDEFINED;
        u4* registerTypes= nullptr;//char* is better?
        JumpNode(u4 ins_pos, u4 *registerTypes):  ins_pos(
                ins_pos), registerTypes(registerTypes) {
        }
    };
    class NodeCache{
        std::vector<JumpNode> nodes;
    public :
        explicit NodeCache()= default;
        inline void addNode(u4 ins_pos,u4* registerTypes){
            nodes.emplace_back(ins_pos,registerTypes);
            //assert(top().registerTypes==registerTypes);
        }
        inline bool isEmpty(){
            return nodes.empty();
        }
        inline JumpNode popNode(){
            if(isEmpty())
                throw std::runtime_error("No node can be popped");
            JumpNode node=top();
            nodes.pop_back();
            return node;
        }
        inline JumpNode& top(){
            return nodes.back();
        }

    };
    class Range;
    class RangeManager{
        const u4 regSize;
        Range* rootRange= nullptr;
        Range* curRange= nullptr;
        u4* regTypes;
        static bool mergeRegTypes(u4 *oldRegs, u4 *newRegisters, u4 size);
    public:
        RangeManager(CodeResolver* _resolver,u4* regTypes_):regSize(_resolver->codeItem->registers_size_){
            rootRange=new Range(0,regTypes_);
            curRange=rootRange;
            RangeManager::regTypes=new u4[regSize];
            memcpy(RangeManager::regTypes, regTypes_, regSize << 2);
            //LOGV("RegSize=%d, reg=%p", regSize,regTypes);
        }

        bool startNewRange(u4 end, u4 newStart,u4* regs);
        bool checkRange( u4 newStart, u4 lastPos);
        void checkNextRange(u4 pos);
        void rollBack(u4 &pos);
        inline void setEnd( u4 pos) {
            curRange->end=pos;
        }
        inline void checkReg(u4 index){
            if(index>regSize){
                throw std::out_of_range(formMessage("reg index out of range,index=",index,",max=",regSize));
            }
        }
        inline u4& regAt(u4 index){
            checkReg(index);
            return regTypes[index];
        }
        inline bool rangeJump(u4 end, u4 newStart){
            u4* newReg=new u4[regSize];
            memcpy(newReg,regTypes,regSize<<2);
            return startNewRange(end,newStart,newReg);
        }
        inline void checkPos(u4 pos){
            if(pos>=curRange->start&&pos<=curRange->end){
                return;
            }
            printRange();
            throw std::runtime_error(formMessage(pos," is not in curRange from ",curRange->start," to ",curRange->end));
        }

        inline void printRange(){
            rootRange->printRange();
        }

        ~RangeManager(){
            delete[] regTypes;
            delete rootRange;
        }
    };
    class Range{
        friend class RangeManager;
        u4 start=0;
        u4 end=UNDEFINED;
        const u4* startRegs;//char* is better?
        Range* preRange= nullptr;
        Range* nextRange= nullptr;
        Range()= delete;
        Range* copy(){
            return new Range(start,end,preRange,nextRange,startRegs);
        }
        Range(u4 start,u4 end,Range* preRange,Range* nextRange,const u4* regs):start(start),end(end),preRange(preRange),nextRange(nextRange),startRegs(regs){}
    public:
        explicit Range(u4 start,const u4* regs):start(start),startRegs(regs){};
        ~Range(){
            delete[] startRegs;
            delete nextRange;
        }
        inline u4 getEnd(){ return end;}
        void printRange(){
            Range* range=this;
            std::string msg;
            do{
                msg=std::move(formMessage(msg,"Range from ",range->start," to ",range->end,'\n'));
            }while ((range=range->nextRange)!= nullptr);
            LOGV("%s",&msg[0]);
        }
    };


    struct Handler{
        u4 typeIdx;
        u4 offset;
    };
    struct TryItemKey{
        u4 start;
        u4 end;
    };
    struct TryItemValue{
        u4 handlerSize;
        Handler* handlers= nullptr;
        ~TryItemValue(){delete [] handlers; }
    };
    class TryMap{
        u4 start=UNDEFINED;
        u4 end=0;
        TryItemValue* values= nullptr;
        typedef std::unordered_map<u4,TryItemValue*> TryHashMap;
        TryHashMap* startMap= nullptr;
        TryHashMap* endMap= nullptr;
    public:
        TryMap(TryItemKey* keys,TryItemValue* values,int size);
        void seekTry(u4 pos,TryItemValue* values[2]);
        ~TryMap(){
            delete [] values;
            delete startMap;
            delete endMap;
        }
    };
    TryMap* tryMap= nullptr;
    const art::DexFile::TypeList* protoList;
    const art::DexFile::MethodId* methodId;
    const art::DexFile::CodeItem* codeItem;
    bool isInstance;
    CodeResolver(){}
    CodeResolver(const CodeResolver& copy)= delete;
    CodeResolver& operator=(CodeResolver&)= delete;

    u4 getVMethodFromIndex(u4 classIdx, u4 vIdx);

    u4 getFiledFromOffset(u4 classIdx, u4 fieldOffset);
    void initTries();
    void initRegisters(u4* registers);
    static void threadInit();
    static void* runResolver(void* args);
    static void threadDestroy();

    static void handleExecuteInline(u2 insns[], u4 pos, u1 *ins);

    static void verifyRegCount(u2 insns[], u4 pos);
    static bool forkNode(const art::DexFile::CodeItem *code, NodeCache &nodeCache,
                            RangeManager &rangeManager, u4 lastPos, u4 newPos);

     void alterField(RangeManager &rangeManager,
                     u2 *insns, u1 rOb, u4 pos);

    void changeIdx( u2 *insns, u4 pos, u4 Idx) const;
public:
    u4 fileOffset;

    CodeResolver(JNIEnv *env_, const art::DexFile::CodeItem *codeItem,
                 const art::DexFile::MethodId *methodId_, bool isInstance_) : methodId(methodId_),
                                                                              protoList(
                                                                                      reinterpret_cast<const
            art::DexFile::TypeList*>(dexGlobal.dexFile->proto_ids_[methodId_->proto_idx_].parameters_off_!=0?dexGlobal.dexFile->
                    begin_+dexGlobal.dexFile->proto_ids_[methodId_->proto_idx_].parameters_off_: nullptr)
    ), isInstance(isInstance_), codeItem(codeItem){}
    bool pend();

    ~CodeResolver(){
        delete tryMap;
    }

    static bool checkAndReplaceOpCodes(u2 *insns, u4 insns_size);
    static void resetInlineTable();
};

#endif
#endif //HOOKMANAGER_CODERESOLVER_H

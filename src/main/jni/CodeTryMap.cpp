//
// Created by Karven on 2016/10/14.
//

#include "Tools.h"
#include "CodeResolver.h"

CodeResolver::TryMap::TryMap(TryItemKey* keys,TryItemValue* values,int size):values(values){
    startMap=new TryHashMap;
    endMap=new TryHashMap;
    for(int i=0;i<size;++i){
        if(keys[i].start<this->start){
            this->start=keys[i].start;
        }
        if(keys[i].end>this->end){
            this->end=keys[i].end;
        }
        if(isLog){
            std::string msg=formMessage("Try from ",keys[i].start," to ",keys[i].end," has \n");
            for(int j=0;j<values[i].handlerSize;++j){
                msg=formMessage(msg,"handler ",j+1," at ",values[i].handlers[j].offset,'\n');
            }
            LOGV("%s",&msg[0]);
        }

        startMap->emplace(keys[i].start,&values[i]);
        endMap->emplace(keys[i].end,&values[i]);
    }

    delete [] keys;
}

void CodeResolver:: initTries() {
    if(codeItem->tries_size_==0) return;
    TryItemValue* tryItemValues= new TryItemValue[codeItem->tries_size_];
    TryItemKey* tryItemKeys=new TryItemKey[codeItem->tries_size_];
    int size;
    u1* tryStart= (u1*)codeItem+(int)sizeof(art::DexFile::CodeItem) - 2 + codeItem->insns_size_in_code_units_ * 2;
    if((codeItem->insns_size_in_code_units_&1) ==1)tryStart+=2;
    const u1* handlerStart= tryStart + (int)sizeof(art::DexFile::TryItem) * codeItem->tries_size_;
    for(int i=0;i<codeItem->tries_size_;++i){
        art::DexFile::TryItem* dexTryItem= reinterpret_cast<art::DexFile::TryItem*>(
                tryStart+i* sizeof(art::DexFile::TryItem));
        TryItemKey* itemKey= &tryItemKeys[i];
        itemKey->start = dexTryItem->start_addr_;//as try must be wrapped into a brace
        itemKey->end=dexTryItem->insn_count_+itemKey->start;
        const u1* ptr=handlerStart+dexTryItem->handler_off_;
        int hCount=readSignedLeb128(size,ptr);
        bool hasCatchAll=hCount<=0;
        if(hCount<0)hCount=-hCount;
        TryItemValue* itemValue= &tryItemValues[i];
        itemValue->handlerSize= (u4) (hasCatchAll + hCount);
        itemValue->handlers=new Handler[itemValue->handlerSize];
        for(int j=0;j<hCount;++j){
            int typeIdx=readUnsignedLeb128(size,ptr);
            int address=readUnsignedLeb128(size,ptr);
            if((u4)address>codeItem->insns_size_in_code_units_){
                throw std::out_of_range(formMessage("Bad try handler address=", address, ",max=",
                                                    codeItem->insns_size_in_code_units_));
            }
            itemValue->handlers[j].typeIdx= (u4) typeIdx;
            itemValue->handlers[j].offset= (u4) address;
        }
        if(hasCatchAll){
            itemValue->handlers[hCount].typeIdx=TypeException;
            itemValue->handlers[hCount].offset= (u4) readUnsignedLeb128(size,ptr);
        }
    }

    tryMap=new TryMap(tryItemKeys,tryItemValues,codeItem->tries_size_);
}
void CodeResolver::TryMap::seekTry(u4 pos,TryItemValue* values[2]) {
    //static_assert(sizeof(values[0])==sizeof(uintptr_t));
    values[0]= nullptr;values[1]= nullptr;
    if(pos<start||pos>end) return ;
    auto iterator= startMap->find(pos);
    if(iterator!=startMap->end())values[0]= iterator->second;
    iterator=endMap->find(pos);
    if(iterator!=endMap->end()) values[1]=iterator->second;
}
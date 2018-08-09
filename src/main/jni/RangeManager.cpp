#include "CodeResolver.h"
#include "support/OpCodesAndType.h"
bool  CodeResolver::RangeManager::startNewRange(u4 end,u4 newStart,u4* regs){
    assert(regs!= nullptr);
    //LOGV("Into %s,newStart=%p",__FUNCTION__,regs,newStart);
    Range* range=curRange;
    if(range== nullptr){
        curRange=rootRange;
        curRange->end=newStart-1;
        Range* newRange=new Range(newStart,regs);
        newRange->preRange=curRange;
        curRange->nextRange=newRange;
        memcpy(regTypes,regs,regSize<<2);
        return true;
    }
    checkPos(end);
    if(range->start > end){
        LOGW("Illegal range,end=%u less than start=%u", end, range->start);
        printRange();
    }
    range->end=end;
    range=rootRange;
    do{
        if(newStart<=range->end&&newStart>=range->start){
            //LOGV("run2 %s",__FUNCTION__);
            if(newStart==range->start){
                if(mergeRegTypes(const_cast<u4 *>(range->startRegs), regs, regSize)){
                    curRange=range;
                    curRange->end=curRange->nextRange== nullptr?UNDEFINED:curRange->nextRange->start-1;
                    return true;
                }
            } else delete[] regs;
            //memcpy(regTypes,regs,regSize<<2);
            return false;
        }
        if(range->nextRange== nullptr)break;//real jump out point
        
        if(newStart<range->nextRange->start&&newStart>range->end){
            Range* newRange=new Range(newStart,range->nextRange->start-1,range,range->nextRange,regs);
            range->nextRange->preRange=newRange;
            range->nextRange=newRange;
            curRange=newRange;
            memcpy(regTypes,regs,regSize<<2);
            return true;//insert range if new start go back;
        }
        range=range->nextRange;
    }while (true);
    //Now new range must be higher than end;
    if(newStart<=end){
        LOGW("Unexpected:New Start=%u less than end=%u",newStart,end);
    }
    Range* newRange=new Range(newStart,regs);
    newRange->preRange= range;
    range->nextRange=newRange;
    curRange=newRange;
    memcpy(regTypes,regs,regSize<<2);
    return true;
};
bool CodeResolver::RangeManager::checkRange( u4 newStart, u4 lastPos) {
    u4 endValue =curRange->end;
    curRange->end = lastPos;
    Range* range=rootRange;
    do {
        if(newStart<=range->end&&newStart>=range->start){
            curRange->end = endValue;
            return false;
        }
    } while ((range = range->nextRange) != nullptr);
    curRange->end = endValue;
    return true;
}

void CodeResolver::RangeManager::checkNextRange(u4 pos){
    if(curRange->nextRange== nullptr) return ;
    if(curRange== nullptr){
        LOGE("curRange is empty");
        printRange();
        return;
    }
    if(curRange->end!=curRange->nextRange->start-1){
        throw std::runtime_error(
                formMessage("Not concatated range!,curEnd=", curRange->end,
                            ",newStart=", (printRange(),curRange->nextRange->start )));
    }
    if (pos>=curRange->nextRange->start){
        curRange=curRange->nextRange;
        if(curRange->nextRange!= nullptr)
            curRange->end=curRange->nextRange->start-1;//should it just be ended?
    }
}
void CodeResolver::RangeManager::rollBack(u4 &pos) {
    if(curRange==rootRange){
        curRange= nullptr;
        pos=0;
    } else{
        Range* range=curRange;
        curRange=curRange->preRange;
        range->nextRange= nullptr;
        delete range;
        curRange->nextRange= nullptr;
        pos=curRange->end;
    }
    
}

bool CodeResolver::RangeManager::mergeRegTypes(u4 *oldRegs, u4 *newRegisters, u4 regSize) {
    bool merged=false;
    for (int i = regSize-1 ;i!=-1; --i) {
        u4 oldType = oldRegs[i];
        u4 newType = newRegisters[i];
        if(oldType != newType){
            if(newType < TypePrimitiveExtend){
                switch (oldType){
                    case TypeException:
                        LOGW("Unexpected TypeException at merge");
                    case  TypeZero:
                        oldRegs[i]=newType;
                        merged=true;
                        break;
                    default:{break;}
                }
            }
        }
    }
    delete [] newRegisters;
    return merged;
}
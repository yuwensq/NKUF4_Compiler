#include "LoopVectorization.h"
using namespace std;

void LoopVectorization::assignLoopBody(unordered_map<Function* ,vector<BasicBlock*>>& vectorLoop){
    for(auto vl:vectorLoop){
        // this->vectorLoop.insert(make_pair(vl.first,vl.second));
        // vectorLoop[vl.first]=vl.second;
        vector<BasicBlock*> temp;
        temp.swap(vl.second);
        this->vectorLoop[vl.first]=temp;
    }
}

void LoopVectorization::printVectorLoopBody(){
    for(auto vl:vectorLoop){
        std::cout<<vl.first->getSymPtr()->toStr()<<endl;
        for(auto bb:vl.second){
            std::cout<<bb->getNo()<<std::endl;
        }
    }
}

void LoopVectorization::pass(){
    for(auto vl:vectorLoop){
        for(auto bodybb:vl.second){
            //找归纳变量add的后继（前续）
            BasicBlock* usefulSucc=nullptr;
            for(auto succbb:bodybb->getSucc()){
                for(auto prebb:bodybb->getPred()){
                    if(succbb->getNo()==prebb->getNo()){
                        usefulSucc=succbb;
                        break;
                    }
                }
                if(usefulSucc){
                    break;
                }
            }
            //begin-end
            vector<pair<Operand*,Operand*>> ivs;
            pair<Operand*,Operand*> coreIv;
            Instruction* CoreIn=nullptr;
            int periodInsNum=0;
            vector<vector<Instruction*>> periodIns(4);
            for(auto ins=usefulSucc->begin();ins!=usefulSucc->end();ins=ins->getNext()){
                if(ins->isAdd()&&ins->getUse()[1]->getEntry()->isConstant()){
                    if(((ConstantSymbolEntry*)ins->getUse()[1]->getEntry())->getValue()==0){
                        ivs.push_back(make_pair(ins->getDef(),ins->getUse()[0]));
                    }
                }
            }

            // cout<<bodybb->getNo()<<endl;
            // for(auto &iv:ivs){
            //     cout<<iv.first->toStr()<<" "<<iv.second->toStr()<<endl;
            // }
            if(ivs.size()>1) return;

            // 4倍展开且规律为i+1，我们可以找临界线，也就是+1
            int putCallNum=0;
            int idx = 0;
            auto j_inst = bodybb->rbegin();
            vector<Instruction *>CoreIns;
            if (j_inst->isCond()) j_inst = j_inst->getPrev();
            auto ins=bodybb->begin();
            for(;ins!=j_inst;ins=ins->getNext()){
                if(ins->isCall()){
                    IdentifierSymbolEntry *funcSE = (IdentifierSymbolEntry *)(((CallInstruction *)ins)->getFunc());
                    //判定是输出
                    if (funcSE->isSysy() && funcSE->getName() != "llvm.memset.p0i8.i32" && !ins->getDef()){
                        putCallNum++;
                        if(putCallNum==2){
                            return;
                        }
                    }
                }
                periodInsNum++;
                periodIns[idx].push_back(ins);
                if(ins->isAdd()&&ins->getUse()[1]->getEntry()->isConstant()){
                    if(((ConstantSymbolEntry*)ins->getUse()[1]->getEntry())->getValue()==1){
                        for(auto &iv:ivs){
                            if(iv.first->toStr()==ins->getUse()[0]->toStr()){
                                coreIv=make_pair(iv.first,iv.second);
                                CoreIn=ins;
                                CoreIns.push_back(ins);
                                break;
                            }
                        }
                        if(CoreIn){
                            break;
                        }
                    }                        
                }   
            }
            //排查
            for(auto in=bodybb->begin();in!=CoreIn;in=in->getNext()){
                if(in->isGep()){
                    Operand* off=in->getUse()[in->getUse().size()-1];
                    if(off->toStr()!=coreIv.first->toStr()){
                        return;
                    }
                }
            }
            bool isNormal=true;
            Instruction* lastAdd=j_inst->getPrev();
            if(lastAdd->isAdd()&&lastAdd->getUse()[0]->toStr()==CoreIn->getDef()->toStr()){
                SymbolEntry* se=lastAdd->getUse()[1]->getEntry();
                if(se->isConstant()&&((ConstantSymbolEntry*)se)->getValue()==3){
                    ins=ins->getNext();
                    isNormal=false;
                    bodybb->remove(CoreIn);
                    ((ConstantSymbolEntry*)se)->setValue(4);
                    lastAdd->replaceUse(lastAdd->getUse()[0],coreIv.first);

                    CoreIn=lastAdd;
                    CoreIns.clear();
                    CoreIns.push_back(CoreIn);
                    periodIns[0].pop_back();
                    periodInsNum--;
                }
            }
            //正常有4个+1
            if(isNormal){
                for (size_t idx = 1; idx < 4; idx++)
                {
                    for (size_t i = 0; i < periodInsNum; i++)
                    {
                        ins = ins->getNext();
                        periodIns[idx].push_back(ins);
                    }
                    CoreIns.push_back(ins);
                }
                // cout<<coreIv.first->toStr()<<endl;
                // cout<<"periodInsNum: "<<periodInsNum<<endl;

                //指令位置转换
                //4倍复制,指令提前
                for (auto &&ins : CoreIns)
                {
                    bodybb->remove(ins);
                    bodybb->insertBefore(ins, j_inst);
                }
                
                for (size_t i = 0; i < periodInsNum-1; i++)
                {
                    for (size_t j = 0; j < 4; j++)
                    {
                        bodybb->remove(periodIns[j][i]);
                        bodybb->insertBefore(periodIns[j][i], j_inst);
                    }
                }  
            }
            // 优化后一个+1，一个+3
            else{
    
                for (size_t idx = 1; idx < 4; idx++)
                {
                    for (size_t i = 0; i < periodInsNum; i++)
                    {
                        periodIns[idx].push_back(ins);
                        ins = ins->getNext();
                    }
                }
                // for (size_t idx = 0; idx < 4; idx++)
                // {
                //     for (size_t i = 0; i < periodInsNum; i++)
                //     {
                //         ins=periodIns[idx][i];
                //         if(ins->isStore()) cout<<"store"<<endl;
                //         else cout<<ins->getDef()->toStr()<<endl;
                //     }
                // }
                // cout<<coreIv.first->toStr()<<endl;
                // cout<<"periodInsNum: "<<periodInsNum<<endl;

                // //指令位置转换
                // //4倍复制,指令提前                
                for (size_t i = 0; i < periodInsNum; i++)
                {
                    for (size_t j = 0; j < 4; j++)
                    {
                        bodybb->remove(periodIns[j][i]);
                        bodybb->insertBefore(periodIns[j][i], j_inst);
                    }
                }  
            }

        }
    }
}
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

            cout<<bodybb->getNo()<<endl;
            for(auto &iv:ivs){
                cout<<iv.first->toStr()<<" "<<iv.second->toStr()<<endl;
            }
            if(ivs.size()>1) return;

            // 4倍展开且规律为i+1，我们可以找临界线，也就是+1
            int idx = 0;
            auto j_inst = bodybb->rbegin();
            vector<Instruction *>CoreIns;
            if (j_inst->isCond()) j_inst = j_inst->getPrev();
            auto ins=bodybb->begin();
            for(;ins!=j_inst;ins=ins->getNext()){
                // if (ins==j_inst) break;
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
            // periodInsNum /= 4;
            cout<<"periodInsNum: "<<periodInsNum<<endl;

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
    }
}
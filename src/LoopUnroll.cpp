#include "LoopUnroll.h"
using namespace std;

void LoopUnroll::calculateCandidateLoop(vector<vector<BasicBlock*>> LoopList){
    // 找出所有的非内部循环
    std::vector<std::vector<BasicBlock*>> UnInnerLoop;
    for(auto loopi:LoopList){
        bool unInner =true;
        for(auto loopj:LoopList){
            if(isSubset(loopi,loopj)){
                unInner=false;
                break;
            }
        }
        if(unInner){
            UnInnerLoop.push_back(loopi);
        }
    }

    //找出非内部循环中仅一个基本块的循环（phi处理后是这样的）,封装后均压入candidateLoops
    for(auto loop1:UnInnerLoop){
        if(loop1.size()!=1){
            continue;
        }
        //下面的是UnInnerLoop.size==1()的，也就是循环就一个基本块
        BasicBlock* cond=nullptr;
        BasicBlock* body=loop1[0];
        //前驱只能有俩，因为是自循环，所以一个前驱必定是自己；另一个就是外部的一个基本块
        assert(body->getPred().size()==2);
        for(auto bb:body->getPred()){
            if(bb!=body){
                cond=bb;
                break;
            }
        }

        loop* l = new loop();
        l->setbody(body);
        l->setcond(cond);
        candidateLoops.push_back(l);
        //cout<<l->getbody()->getNo()<<endl;
    }
}

bool LoopUnroll::isSubset(vector<BasicBlock*> son, vector<BasicBlock*> farther){
    for(auto sonBB:son){
        if(!count(farther.begin(),farther.end(),sonBB)){
            return false;
        }
    }
    //避免son和farther是同一个循环
    return son.size()!=farther.size();
}

void LoopUnroll::Unroll(){
    for(auto loop:candidateLoops){
        //包含call指令的不展开
        bool hasCall = false;
        BasicBlock* body = loop->getbody();
        for(auto bodyinstr = body->begin(); bodyinstr != body->end(); bodyinstr = bodyinstr->getNext()){
            if(bodyinstr->isCall()){
                hasCall = true;
                break;
            }
        }
        if(hasCall){
            continue;
        }

    }
}
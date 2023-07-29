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
}

bool LoopUnroll::isSubset(vector<BasicBlock*> son, vector<BasicBlock*> farther){
    
}

void LoopUnroll::Unroll(){

}
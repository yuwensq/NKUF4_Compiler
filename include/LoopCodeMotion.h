#ifndef __LOOP_CODE_MOTION_H__
#define __LOOP_CODE_MOTION_H__
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <assert.h>
#include "Unit.h"
#include "Function.h"
#include "BasicBlock.h"
#include "Operand.h"

//循环优化类
class LoopCodeMotion{
    Unit *unit;
    //针对每一个基本块，求他们的必经节点，存入DomBBSet
    std::unordered_map<Function*, std::unordered_map<BasicBlock*,std::vector<BasicBlock*>>> DomBBSet;
    //循环中不变的常量（以操作数形式）
    std::map<Function*, std::map<std::vector<BasicBlock*>,std::set<Operand*>>> LoopConst;
public:
    //代码外提
    LoopCodeMotion(Unit* unit):unit(unit){};
    void initializeDomBBSet(Function* func);
    bool ifDomBBSetChange(std::unordered_map<BasicBlock*,std::vector<BasicBlock*>>& lastSet,Function* func);
    std::unordered_map<BasicBlock*,std::vector<BasicBlock*>>& getDomBBSet(Function* func){return DomBBSet[func];};
    std::vector<BasicBlock*> getIntersectBBList(std::vector<BasicBlock*>& List1,std::vector<BasicBlock*>& List2);
    void calculateFinalDomBBSet(Function* func);
    std::vector<std::pair<BasicBlock*,BasicBlock*>> getBackEdges(Function* func);
    std::vector<std::vector<std::pair<BasicBlock*,BasicBlock*>>> mergeEdge(std::vector<std::pair<BasicBlock*,BasicBlock*>>& BackEdges);
    std::vector<std::vector<BasicBlock*>> calculateLoopList(Function* func,std::vector<std::vector<std::pair<BasicBlock*,BasicBlock*>>> &edgeGroups);
    bool OperandIsLoopConst(Operand * op,std::vector<BasicBlock*>Loop,std::vector<Instruction*> LoopConstInstructions);
    std::vector<Instruction*> calculateLoopConstant(std::vector<BasicBlock*>Loop,Function* func);
    std::vector<BasicBlock*> calculateOutBlock(std::vector<BasicBlock*>& Loop);
    void changePhiInstruction(std::vector<BasicBlock*>& Loop,BasicBlock* newPreBlock,std::vector<BasicBlock*> oldBlocks);
    void CodePullUp(Function* func,std::vector<std::vector<BasicBlock*>>& LoopList,std::vector<std::pair<BasicBlock*,BasicBlock*>>& BackEdges);
    void dealwithNoPreBB(Function* func);

    void printDomBB(Function * func);
    void printBackEdges(std::vector<std::pair<BasicBlock*,BasicBlock*>> BackEdges);
    void printEdgeGroups(std::vector<std::vector<std::pair<BasicBlock*,BasicBlock*>>> edgeGroups);
    void printLoop(std::vector<std::vector<BasicBlock*>>& LoopList);
    void printLoopConst(std::vector<Instruction*> LoopConstInstructions);

    void pass();
};

#endif


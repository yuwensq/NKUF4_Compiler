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

/// @brief corresponding coefficients
struct CCEntry
{
    /**i=>(i,1,0), 
     * j = 3*i+2; j=>(i,3,2)
     * j=>(i,c,d)
     * k = j + b; then k=>(i,c,d+b)
     * k = b - j; then k=>(i,-c,b-d)
     * k = b * j; then k=>(i,b*c,b*d)
     */
    Operand *basic_induction_variable;
    // Operand *multiplicative_factor, *additive_factor;
    int multiplicative, additive;
};
/**
 * TODO：
 * Identify loop const operands
 * new a latch, which will affect LoopList
 * the order of LoopList
 * ************
 * int a; a[2]=0; 不报错
 * 两个return，报错
 */

// 循环优化类
class LoopCodeMotion
{
    Unit *unit;
    // 针对每一个基本块，求他们的必经节点，存入DomBBSet
    std::unordered_map<Function *, std::unordered_map<BasicBlock *, std::vector<BasicBlock *>>> DomBBSet;
    // 循环中不变的常量（以操作数形式）
    std::map<Function *, std::map<std::vector<BasicBlock *>, std::set<Operand *>>> LoopConst;
    std::set<Operand *> loopStoreGlobal; // 每一个loop中，所有store语句的use[0]操作数
    std::set<std::vector<Operand *>> loopStoreGep;
    std::set<Operand *> loopStoreGepDef;
    std::set<Instruction *> loopCallIns;

public:
    // 代码外提
    LoopCodeMotion(Unit *unit) : unit(unit){};
    void initializeDomBBSet(Function *func);
    bool ifDomBBSetChange(std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> &lastSet, Function *func);
    std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> &getDomBBSet(Function *func) { return DomBBSet[func]; };
    std::vector<BasicBlock *> getIntersectBBList(std::vector<BasicBlock *> &List1, std::vector<BasicBlock *> &List2);
    void calculateFinalDomBBSet(Function *func);
    std::vector<std::pair<BasicBlock *, BasicBlock *>> getBackEdges(Function *func);
    std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> mergeEdge(std::vector<std::pair<BasicBlock *, BasicBlock *>> &BackEdges);
    std::vector<std::vector<BasicBlock *>> calculateLoopList(Function *func, std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> &edgeGroups);
    bool OperandIsLoopConst(Operand *op, std::vector<BasicBlock *> Loop, std::vector<Instruction *> LoopConstInstructions, Instruction *gepIns = nullptr);
    std::vector<Instruction *> calculateLoopConstant(std::vector<BasicBlock *> Loop, Function *func);
    std::vector<BasicBlock *> calculateOutBlock(std::vector<BasicBlock *> &Loop);
    void changePhiInstruction(std::vector<BasicBlock *> &Loop, BasicBlock *newPreBlock, std::vector<BasicBlock *> oldBlocks);
    void CodePullUp(Function *func, std::vector<std::vector<BasicBlock *>> &LoopList, std::vector<std::pair<BasicBlock *, BasicBlock *>> &BackEdges);
    void dealwithNoPreBB(Function *func);
    bool isLoadInfluential(Instruction *ins);

    void printDomBB(Function *func);
    void printBackEdges(std::vector<std::pair<BasicBlock *, BasicBlock *>> BackEdges);
    void printEdgeGroups(std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> edgeGroups);
    void printLoop(std::vector<std::vector<BasicBlock *>> &LoopList);
    void printLoopConst(std::vector<Instruction *> LoopConstInstructions);

    void clearData();
    void pass();
    bool pass1(); // 成功展开返回true
    // 强度削弱
private:
    std::unordered_map<Operand *, CCEntry> IVs;
    std::unordered_map<Operand *, bool> lcOps; // loop const int
    Operand *one = nullptr, *zero = nullptr;

public:
    void pass2();
    void preprocess(
        BasicBlock *preheader,
        std::vector<BasicBlock *> &Loop,
        std::vector<std::pair<BasicBlock *, BasicBlock *>> &BackEdges);
    void LoopStrengthReduction(BasicBlock *preheader, std::vector<BasicBlock *> &Loop);
    void findNonbasicInductionVariables(std::vector<BasicBlock *> &Loop);
    void removeSelfIVs(Function *func);
};

#endif

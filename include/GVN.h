#ifndef __GVN_H__
#define __GVN_H__

#include "Unit.h"
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <vector>

/**
 * && ||
 * constExp
 * 提进来的rank 变不变
 * 增殖的顺序？
 */

bool isSameRHS(Instruction *, Instruction *);

struct Cmp
{
    bool operator()(Instruction *const &a, Instruction *const &b) const
    {
        assert(a->isBinary() && b->isBinary());
        if (isSameRHS(a, b))
            return false;
        if (a->getOpCode() == b->getOpCode())
        {
            return (a->getOperands()[1] < b->getOperands()[1]) || (a->getOperands()[1] == b->getOperands()[1] && a->getOperands()[2] < b->getOperands()[2]);
        }
        else
        {
            return a->getOpCode() < b->getOpCode();
        }
    }
};

class GlobalValueNumbering
{
private:
    Unit *unit;
    std::vector<BasicBlock *> rtop;
    // std::vector<std::pair<BasicBlock *, BasicBlock *>> loopexits;
    std::unordered_set<BasicBlock *> loopheaders;
    std::unordered_set<BasicBlock *> landingpads;
    std::unordered_map<BasicBlock *, std::unordered_set<BasicBlock *>> virtualEdge;
    std::unordered_map<BasicBlock *, std::unordered_set<BasicBlock *>> virtualREdge;
    std::unordered_map<Operand *, int> ranks;
    int max_rank;
    std::vector<Instruction *> trivial_worklist;
    std::unordered_map<BasicBlock *, std::map<Instruction *, std::vector<Instruction *>, Cmp>> LCTs;
    std::map<std::pair<BasicBlock *, BasicBlock *>, std::vector<Instruction *>> MCTs;
    std::map<std::pair<BasicBlock *, BasicBlock *>, std::vector<Instruction *>> MCTps;
    int cur_rank;

public:
    GlobalValueNumbering(Unit *unit) : unit(unit) {}
    void pass();

    void preprocess(Function *);
    void assignRanks(Instruction *);
    void removeTrivialAssignments();
    void eliminateLocalRedundancies();

    void eliminateRedundancies(Function *);
    void moveComputationsFromSuccessors(BasicBlock *);
    void identifyMovableComputations(BasicBlock *);
    void identifyMovableComputations2(BasicBlock *);
    bool questionPropagation(Instruction *);
    bool qpLocalSearch(Instruction *);
    bool qpGlobalSearch(std::vector<Instruction *> &may_list, Instruction *);
    // std::vector<std::pair<BasicBlock *, Instruction *>> renameExpression(Instruction *);
    void moveComputationsOutOfALoop(BasicBlock *);
    void moveComputationsIntoPad(BasicBlock *);
    void eliminateGlobalRedundancies(Instruction *);
};

#endif

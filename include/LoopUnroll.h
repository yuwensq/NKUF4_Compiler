#ifndef __LOOPUNROLL_H__
#define __LOOPUNROLL_H__

#include "Unit.h"
#include "BasicBlock.h"
#include "Instruction.h"
#include "Operand.h"
#include <vector>
#include <stack>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <iostream>
using namespace std;

class loop
{
    BasicBlock *cond;
    BasicBlock *body;

public:
    void setcond(BasicBlock *cond) { this->cond = cond; };
    void setbody(BasicBlock *body) { this->body = body; };
    BasicBlock *getcond() { return cond; };
    BasicBlock *getbody() { return body; };
};

class LoopUnroll
{
    unordered_map<Function *, unordered_map<BasicBlock *, vector<BasicBlock *>>> DomBBSet;
    vector<loop *> candidateLoops;
    int MAXUNROLLNUM = 400;
    int UNROLLNUM = 4;
    int MAXUNROLLINSNUM = 4000;

public:
    LoopUnroll(unordered_map<Function *, unordered_map<BasicBlock *, vector<BasicBlock *>>> DomBBSet) : DomBBSet(DomBBSet){};
    void calculateCandidateLoop(vector<vector<BasicBlock *>> LoopList);
    bool isSubset(vector<BasicBlock *> son, vector<BasicBlock *> farther);

    bool isRegionConst(Operand *i, Operand *c);
    Operand *getBeginOp(BasicBlock *bb, Operand *strideOp, stack<Instruction *> &Insstack);
    void specialUnroll(BasicBlock *bb, int count, Operand *endOp, Operand *strideOp, bool ifall);
    void normalUnroll(BasicBlock *condbb, BasicBlock *bodybb, Operand *beginOp, Operand *endOp, Operand *strideOp, bool isIncrease = true);

    void Unroll();
    bool successUnroll = false;
};

#endif
#ifndef __LOOP_VECTORIZATION_H__
#define __LOOP_VECTORIZATION_H__

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

class LoopVectorization{
    unordered_map<Function* ,vector<BasicBlock*>> vectorLoop;
    Unit* unit;
    vector<vector<Operand*>> gepOps;

public:
    LoopVectorization(Unit* unit):unit(unit){};
    void assignLoopBody(unordered_map<Function* ,vector<BasicBlock*>>& vectorLoop);
    void printVectorLoopBody();
    void pass();
};

#endif
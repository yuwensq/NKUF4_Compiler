#ifndef __FUNCTION_INLINE_H_
#define __FUNCTION_INLINE_H_

#include "Unit.h"

class FunctionInline
{
private:
    Unit *unit;
    // 存函数的被调用语句
    std::map<Function *, std::set<Instruction *>> callIns;
    BasicBlock *entryBlock;
    std::map<BasicBlock*, std::pair<Instruction*, Operand*>> exitBlocks;
    void clearData();
    void preProcess();
    bool shouldBeInlined(Function *);
    Operand* copyOp(Operand*);
    void copyFunc(Instruction *, Function*);
    void merge(Function *, Instruction *);
    void doInline(Function *);

public:
    FunctionInline(Unit *unit) : unit(unit) {}
    void pass();
};

#endif
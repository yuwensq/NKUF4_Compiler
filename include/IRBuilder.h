#ifndef __IRBUILDER_H__
#define __IRBUILDER_H__

class Unit;
class Function;
class BasicBlock;

class IRBuilder
{
private:
    Unit *unit;
    BasicBlock *insertBB;   // The current basicblock that instructions should be inserted into.
    bool duplicate = false; // 当前的指令是否已经生成了一次了

public:
    IRBuilder(Unit *unit) : unit(unit){};
    void setInsertBB(BasicBlock *bb) { insertBB = bb; };
    Unit *getUnit() { return unit; };
    BasicBlock *getInsertBB() { return insertBB; };
    void setDuplicate(bool dup) { duplicate = dup; };
    bool isDuplicate() { return duplicate; };
};

#endif
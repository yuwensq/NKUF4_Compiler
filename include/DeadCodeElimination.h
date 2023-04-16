#ifndef __DEAD_CODE_ELIMINATION_H__
#define __DEAD_CODE_ELIMINATION_H__

#include "Unit.h"

class DeadCodeElimination {

private:
    Unit* unit;
    std::vector<Instruction*> worklist; //关键指令

public:
    DeadCodeElimination(Unit* unit) : unit(unit){};
    void initalize(Function* function);
    void mark(Function* function);
    bool remove(Function* function);
    void pass();
    void adjustBlock(Function* function);
};

#endif
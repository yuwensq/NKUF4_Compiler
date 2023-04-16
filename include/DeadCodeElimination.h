#ifndef __DEAD_CODE_ELIMINATION_H__
#define __DEAD_CODE_ELIMINATION_H__

#include "Unit.h"

class DeadCodeElimination {
    Unit* unit;

   public:
    DeadCodeElimination(Unit* unit) : unit(unit){};
    void initalize(Function* function);
    void mark(Function* function);
    bool remove(Function* function);
    void pass();
    void adjustBlock(Function* function);
};

#endif
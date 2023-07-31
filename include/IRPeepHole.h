#ifndef _IR_PEEP_HOLE_H_
#define _IR_PEEP_HOLE_H_

#include "Unit.h"

class IRPeepHole
{
private:
    Unit *unit;
    void subPass(Function *);

public:
    IRPeepHole(Unit *unit) : unit(unit){};
    void pass();
};

#endif
#ifndef _IR_PEEP_HOLE_H_
#define _IR_PEEP_HOLE_H_

#include "Unit.h"

class IRPeepHole
{
private:
    Unit *unit;
    bool flag = false;
    void subPass(Function *);
    void subPassForBlk(Function *);
    void subPass2(Function *);

public:
    IRPeepHole(Unit *unit) : unit(unit){};
    void setFlag(bool flag) { this->flag = flag; };
    void pass();
};

#endif
#ifndef __IRSPARSECONDCONSTPROP_H__
#define __IRSPARSECONDCONSTPROP_H__

#include "Unit.h"

class IRSparseCondConstProp
{
private:
    Unit *unit;

public:
    IRSparseCondConstProp(Unit* unit) : unit(unit) {};
    void pass();
};

#endif
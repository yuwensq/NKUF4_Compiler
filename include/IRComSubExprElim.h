#ifndef __IRCOMSUBEXPRELIM_H__
#define __IRCOMSUBEXPRELIM_H__

#include "Unit.h"

class IRComSubExprElim
{
private:
    Unit *unit;

public:
    IRComSubExprElim(Unit *unit) : unit(unit) {}
    void analyse();
    void pass();
};

#endif
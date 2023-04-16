#include "PhiElim.h"

void PhiElimination::pass()
{
    for (auto it = unit->begin(); it != unit->end(); it++)
    {
        (*it)->de_phi();
    }
}
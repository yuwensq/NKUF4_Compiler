#ifndef __MACHINEPEEPHOLE_H__
#define __MACHINEPEEPHOLE_H__

#include "MachineCode.h"

class MachinePeepHole
{
private:
    MachineUnit *unit;

public:
    MachinePeepHole(MachineUnit *unit) : unit(unit) {}
    void pass();
};

#endif
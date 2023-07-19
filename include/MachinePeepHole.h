#ifndef __MACHINEPEEPHOLE_H__
#define __MACHINEPEEPHOLE_H__

#include "MachineCode.h"

class MachinePeepHole
{
private:
    MachineUnit *unit;
    int pass_times;
    void subPass(bool afterRegAlloc);

public:
    MachinePeepHole(MachineUnit *unit, int pass_times = 0) : unit(unit), pass_times(pass_times) {}
    void pass(bool afterRegAlloc = false);
};

#endif

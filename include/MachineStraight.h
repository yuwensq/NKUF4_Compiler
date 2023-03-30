#ifndef __MACHINESTRAIGHT_H__
#define __MACHINESTRAIGHT_H__

#include "MachineCode.h"
#include <map>

class MachineStraight
{
private:
    MachineUnit *unit;
    std::map<int, std::pair<MachineBlock *, MachineBlock *>> blk2blk;
    void getSlimBlock();
    void removeSlimBlock();

public:
    MachineStraight(MachineUnit *unit) : unit(unit) {}
    void pass();
};

#endif
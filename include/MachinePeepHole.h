#ifndef __MACHINEPEEPHOLE_H__
#define __MACHINEPEEPHOLE_H__

#include "MachineCode.h"

class MachinePeepHole
{
private:
    MachineUnit *unit;
    std::map<MachineOperand, int> op2DefTimes;
    std::map<MachineOperand, int> op2UseTimes;
    void clearData();
    // 预处理
    void analyse();
    bool subPass(bool afterRegAlloc);

public:
    MachinePeepHole(MachineUnit *unit) : unit(unit) {}
    void pass(bool afterRegAlloc = false);
    void pass2();
};

#endif

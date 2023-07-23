#ifndef _MACHINELVN_H_
#define _MACHINELVN_H_

#include "MachineCode.h"

class MachineLVN
{
private:
    MachineUnit *unit;

public:
    MachineLVN(MachineUnit *munit) : unit(munit){};
    void pass();
};

#endif
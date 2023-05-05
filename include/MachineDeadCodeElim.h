#ifndef __MACHINE_DEAD_CODE_ELIM_H_
#define __MACHINE_DEAD_CODE_ELIM_H_

#include "MachineCode.h"

class MachineDeadCodeElim
{
private:
    MachineUnit *munit;

public:
    MachineDeadCodeElim(MachineUnit *munit) : munit(munit){};
    void pass();
};

#endif
#ifndef __MACHINE_TAIL_CALL_HANDLER_H_
#define __MACHINE_TAIL_CALL_HANDLER_H_
#include "MachineCode.h"

// 因为实在懵der，我将以一种极端的方式判断尾调用优化
class MachineTailCallHandler
{
private:
    MachineUnit *munit;

public:
    MachineTailCallHandler(MachineUnit *munit) : munit(munit){};
    void pass();
};

#endif
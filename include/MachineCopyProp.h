#ifndef __MACHINE_COPY_PROP_H_
#define __MACHINE_COPY_PROP_H_

#include "MachineCode.h"
#include <unordered_map>

struct CopyStmt
{
    MachineOperand *dst, *src;
    CopyStmt(MachineInstruction *inst)
    {
        Assert(inst->isMov() || inst->isVMov32(), "这里应该传入一个mov指令");
        dst = inst->getDef()[0];
        src = inst->getUse()[0];
    }
    bool operator==(const CopyStmt &other) const
    {
        return (*dst == *other.dst && *src == *other.src);
    }
};

class MachineCopyProp
{
private:
    MachineUnit *munit;

    std::vector<CopyStmt> allCopyStmts;
    std::unordered_map<MachineInstruction *, int> inst2CopyStmt;

    std::unordered_map<MachineBlock *, std::set<int>> Gen;
    std::unordered_map<MachineBlock *, std::set<int>> Kill;
    std::unordered_map<MachineBlock *, std::set<int>> In;
    std::unordered_map<MachineBlock *, std::set<int>> Out;

    void clearData();
    std::set<int> intersection(std::set<int> &, std::set<int> &);
    bool couldKill(MachineInstruction *, CopyStmt &);
    int getHash(MachineOperand *);
    void calGenKill(MachineFunction *);
    void calInOut(MachineFunction *);
    bool replaceOp(MachineFunction *);
    bool copyProp(MachineFunction *);

public:
    MachineCopyProp(MachineUnit *munit) : munit(munit){};
    void pass();
};

#endif
#ifndef __MACHINESTRAIGHT_H__
#define __MACHINESTRAIGHT_H__

#include "MachineCode.h"
#include <unordered_set>
#include <map>

class MachineStraight
{
private:
    MachineUnit *unit;
    std::map<int, std::pair<MachineBlock *, MachineBlock *>> blk2blk;
    // junctions存可以被合并到父节点的节点
    std::unordered_set<MachineBlock *> junctions;
    void getSlimBlock();
    void removeSlimBlock();
    void getJunctions();
    void mergeJunctions();
    void pass1();
    void pass2();

public:
    MachineStraight(MachineUnit *unit) : unit(unit) {}
    void pass();
    void lastPass();
};

#endif
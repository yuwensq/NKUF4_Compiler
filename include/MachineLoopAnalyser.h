#ifndef _MACHINE_LOOP_ANALYSER_H_
#define _MACHINE_LOOP_ANALYSER_H_

#include "MachineCode.h"

class MachineLoopAnalyser
{
private:
    MachineUnit *unit;
    std::map<MachineBlock *, std::set<MachineBlock *>> doms;
    std::map<MachineBlock *, std::set<MachineBlock *>> backEdges; // first是支配节点
    std::vector<std::set<MachineBlock *>> loops;                  // 是支配节点
    std::map<MachineBlock *, int> depths;
    void clearData();
    void computeDoms(MachineFunction *);
    void lookForBackEdge(MachineFunction *);
    void computeLoops(MachineFunction *);
    void intersection(std::set<MachineBlock *> &, std::set<MachineBlock *> &, std::set<MachineBlock *> &);

public:
    MachineLoopAnalyser(MachineUnit *munit) : unit(munit){};
    void analyse(MachineFunction *);
    int getDepth(MachineBlock *);
};

#endif
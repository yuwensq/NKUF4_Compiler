/**
 * linear scan register allocation
 */

#ifndef _LINEARSCAN_H__
#define _LINEARSCAN_H__
#include <set>
#include <map>
#include <vector>
#include <list>

class MachineUnit;
class MachineOperand;
class MachineFunction;

class LinearScan
{
private:
    struct Interval
    {
        int start;
        int end;
        bool spill; // whether this vreg should be spilled to memory
        int disp;   // displacement in stack
        int rreg;   // the real register mapped from virtual register if the vreg is not spilled to memory
        std::set<MachineOperand *> defs;
        std::set<MachineOperand *> uses;
        bool fpu; // 是不是浮点寄存器
    };
    MachineUnit *unit;
    MachineFunction *func;
    std::vector<Interval *> spillIntervals; // 加这个变量也是希望能够快一点，不过最后好像差不多
    std::vector<int> regs; // 这个存通用寄存器
    std::vector<int> fpuRegs; // 这个存special寄存器
    std::map<MachineOperand *, std::set<MachineOperand *>> du_chains;
    std::vector<Interval *> intervals;
    std::vector<Interval *> active;
    static bool compareStart(Interval *a, Interval *b);
    static bool compareEnd(Interval *a, Interval *b);
    void expireOldIntervals(Interval *interval);
    void spillAtInterval(Interval *interval);
    void makeDuChains();
    void computeLiveIntervals();
    bool linearScanRegisterAllocation();
    void modifyCode();
    void genSpillCode();

public:
    LinearScan(MachineUnit *unit);
    void allocateRegisters();
};

#endif
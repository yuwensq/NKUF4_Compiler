#ifndef _MACHINELVN_H_
#define _MACHINELVN_H_

#include "MachineCode.h"
#include <string>
#include <map>

class MachineLVN
{
private:
    MachineUnit *unit;
    std::map<std::string, int> hashTable;
    std::map<int, MachineOperand> vn2Op;
    bool skip(MachineInstruction *);
    int nextVN;
    std::string toStr(MachineOperand *);
    std::string toStr(MachineInstruction *);
    void clearData();
    void replaceWithMov(std::vector<MachineInstruction *>::iterator);
    void constFold(std::vector<MachineInstruction *>::iterator);
    void addNewExpr(std::vector<MachineInstruction *>::iterator);
    void pass(MachineFunction *);

public:
    MachineLVN(MachineUnit *munit) : unit(munit){};
    void pass();
};

#endif
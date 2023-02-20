#ifndef __ASMBUILDER_H__
#define __ASMBUILDER_H__

#include "MachineCode.h"

class AsmBuilder
{
private:
    MachineUnit *mUnit;         // mahicne unit
    MachineFunction *mFunction; // current machine code function;
    MachineBlock *mBlock;       // current machine code block;
    int cmpOpcode;              // CmpInstruction opcode, for CondInstruction;
public:
    void setUnit(MachineUnit *unit) { this->mUnit = unit; };
    void setFunction(MachineFunction *func) { this->mFunction = func; };
    void setBlock(MachineBlock *block) { this->mBlock = block; };
    void setCmpOpcode(int opcode) { this->cmpOpcode = opcode; };
    MachineUnit *getUnit() { return this->mUnit; };
    MachineFunction *getFunction() { return this->mFunction; };
    MachineBlock *getBlock() { return this->mBlock; };
    int getCmpOpcode() { return this->cmpOpcode; };
    static bool isLegalImm(int imm)
    {
        unsigned int num = (unsigned int)imm;
        for (int i = 0; i < 16; i++)
        {
            if (num <= 0xff)
            {
                return true;
            }
            num = ((num << 2) | (num >> 30));
        }
        return false;
    }
};

#endif
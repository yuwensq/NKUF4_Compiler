#ifndef __INSTRUCTION_H__
#define __INSTRUCTION_H__

#include "Operand.h"
#include "AsmBuilder.h"
#include <vector>
#include <map>
#include <sstream>

class BasicBlock;

class Instruction
{
public:
    Instruction(unsigned instType, BasicBlock *insert_bb = nullptr);
    virtual ~Instruction();
    BasicBlock *getParent();
    bool isUncond() const { return instType == UNCOND; };
    bool isCond() const { return instType == COND; };
    bool isRet() const { return instType == RET; };
    bool isAlloc() const { return instType == ALLOCA; };
    bool isLoad() const { return instType == LOAD; };
    bool isStore() const { return instType == STORE; };
    bool isPhi() const { return instType == PHI; };
    bool isCall() const { return instType == CALL; };
    bool isBinaryCal() const { return instType == BINARY || instType == CMP; };
    bool isUnaryCal() const { return instType == ZEXT || instType == XOR || instType == FPTSI || instType == SITFP; };
    bool isBinary() const { return instType == BINARY; };
    void setParent(BasicBlock *);
    void setNext(Instruction *);
    void setPrev(Instruction *);
    Instruction *getNext();
    Instruction *getPrev();
    virtual void output() const = 0;
    MachineOperand *genMachineOperand(Operand *, AsmBuilder *);
    MachineOperand *genMachineReg(int reg, bool fpu);
    MachineOperand *genMachineVReg(bool fpu);
    MachineOperand *genMachineImm(int val);
    MachineOperand *genMachineLabel(int block_no);
    MachineOperand *immToVReg(MachineOperand *, MachineBlock *);
    virtual void genMachineCode(AsmBuilder *) = 0;
    std::vector<Operand *> &getOperands() { return operands; }

    virtual Operand *getDef() { return nullptr; }
    virtual std::vector<Operand *> getUse() { return {}; }
    std::vector<Operand *> replaceAllUsesWith(Operand *); // Mem2Reg
    virtual void replaceUse(Operand *, Operand *) {}
    virtual void replaceDef(Operand *) {}
    int getOpCode() const { return opcode; }

protected:
    unsigned instType;
    unsigned opcode;
    Instruction *prev;
    Instruction *next;
    BasicBlock *parent;
    std::vector<Operand *> operands;
    enum
    {
        BINARY,
        COND,
        UNCOND,
        RET,
        CALL,
        LOAD,
        STORE,
        CMP,
        ALLOCA,
        XOR,
        ZEXT,
        GEP,
        FPTSI,
        SITFP,
        PHI
    };
};

// meaningless instruction, used as the head node of the instruction list.
class DummyInstruction : public Instruction
{
public:
    DummyInstruction() : Instruction(-1, nullptr){};
    void output() const {};
    void genMachineCode(AsmBuilder *){};
};

class AllocaInstruction : public Instruction
{
public:
    AllocaInstruction(Operand *dst, SymbolEntry *se, BasicBlock *insert_bb = nullptr);
    ~AllocaInstruction();
    void output() const;
    void genMachineCode(AsmBuilder *);
    SymbolEntry *getEntry()
    {
        return se;
    }
    Operand *getDef() { return operands[0]; }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }

private:
    SymbolEntry *se;
};

class LoadInstruction : public Instruction
{
public:
    LoadInstruction(Operand *dst, Operand *src_addr, BasicBlock *insert_bb = nullptr);
    ~LoadInstruction();
    void output() const;
    void genMachineCode(AsmBuilder *);
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse() { return {operands[1]}; }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[1] == old)
        {
            operands[1]->removeUse(this);
            operands[1] = rep;
            rep->addUse(this);
        }
    }
};

class StoreInstruction : public Instruction
{
public:
    StoreInstruction(Operand *dst_addr, Operand *src, BasicBlock *insert_bb = nullptr);
    ~StoreInstruction();
    void output() const;
    void genMachineCode(AsmBuilder *);
    std::vector<Operand *> getUse() { return {operands[0], operands[1]}; }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[0] == old)
        {
            operands[0]->removeUse(this);
            operands[0] = rep;
            rep->addUse(this);
        }
        else if (operands[1] == old)
        {
            operands[1]->removeUse(this);
            operands[1] = rep;
            rep->addUse(this);
        }
    }
};

class BinaryInstruction : public Instruction
{
public:
    BinaryInstruction(unsigned opcode, Operand *dst, Operand *src1, Operand *src2, BasicBlock *insert_bb = nullptr);
    ~BinaryInstruction();
    void output() const;
    void genMachineCode(AsmBuilder *);
    enum
    {
        ADD = 0,
        SUB,
        MUL,
        DIV,
        AND,
        OR,
        MOD
    };
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse() { return {operands[1], operands[2]}; }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[1] == old)
        {
            operands[1]->removeUse(this);
            operands[1] = rep;
            rep->addUse(this);
        }
        else if (operands[2] == old)
        {
            operands[2]->removeUse(this);
            operands[2] = rep;
            rep->addUse(this);
        }
    }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }

private:
    bool floatVersion;
};

class CmpInstruction : public Instruction
{
public:
    CmpInstruction(unsigned opcode, Operand *dst, Operand *src1, Operand *src2, BasicBlock *insert_bb = nullptr);
    ~CmpInstruction();
    void output() const;
    void genMachineCode(AsmBuilder *);
    enum
    {
        E = 0,
        NE,
        GE,
        L,
        LE,
        G
    };
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse() { return {operands[1], operands[2]}; }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[1] == old)
        {
            operands[1]->removeUse(this);
            operands[1] = rep;
            rep->addUse(this);
        }
        else if (operands[2] == old)
        {
            operands[2]->removeUse(this);
            operands[2] = rep;
            rep->addUse(this);
        }
    }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }

private:
    bool floatVersion;
};

// unconditional branch
class UncondBrInstruction : public Instruction
{
public:
    UncondBrInstruction(BasicBlock *, BasicBlock *insert_bb = nullptr);
    void output() const;
    void setBranch(BasicBlock *);
    BasicBlock *getBranch();
    void genMachineCode(AsmBuilder *);

protected:
    BasicBlock *branch;
};

// conditional branch
class CondBrInstruction : public Instruction
{
public:
    CondBrInstruction(BasicBlock *, BasicBlock *, Operand *, BasicBlock *insert_bb = nullptr);
    ~CondBrInstruction();
    void output() const;
    void setTrueBranch(BasicBlock *);
    BasicBlock *getTrueBranch();
    void setFalseBranch(BasicBlock *);
    BasicBlock *getFalseBranch();
    void genMachineCode(AsmBuilder *);
    std::vector<Operand *> getUse() { return {operands[0]}; }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[0] == old)
        {
            operands[0]->removeUse(this);
            operands[0] = rep;
            rep->addUse(this);
        }
    }

protected:
    BasicBlock *true_branch;
    BasicBlock *false_branch;
};

class CallInstruction : public Instruction // 函数调用
{
public:
    CallInstruction(Operand *dst, SymbolEntry *func, std::vector<Operand *> params, BasicBlock *insert_bb = nullptr);
    ~CallInstruction();
    void output() const;
    void genMachineCode(AsmBuilder *);
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse()
    {
        std::vector<Operand *> vec;
        for (auto it = operands.begin() + 1; it != operands.end(); it++)
            vec.push_back(*it);
        return vec;
    }
    void replaceDef(Operand *rep)
    {
        if (operands[0])
        {
            operands[0]->setDef(nullptr);
            operands[0] = rep;
            operands[0]->setDef(this);
        }
    }
    void replaceUse(Operand *old, Operand *rep)
    {
        for (size_t i = 1; i < operands.size(); i++)
        {
            if (operands[i] == old)
            {
                operands[i]->removeUse(this);
                operands[i] = rep;
                rep->addUse(this);
            }
        }
    }

private:
    SymbolEntry *func;
};

class RetInstruction : public Instruction
{
public:
    RetInstruction(Operand *src, BasicBlock *insert_bb = nullptr);
    ~RetInstruction();
    void output() const;
    void genMachineCode(AsmBuilder *);
    std::vector<Operand *> getUse()
    {
        if (operands.size())
            return {operands[0]};
        else
            return {};
    }
    void replaceDef(Operand *rep)
    {
        if (operands.size())
        {
            operands[0]->setDef(nullptr);
            operands[0] = rep;
            operands[0]->setDef(this);
        }
    }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands.size() && operands[0] == old)
        {
            operands[0]->removeUse(this);
            operands[0] = rep;
            rep->addUse(this);
        }
    }
};

class XorInstruction : public Instruction // not指令
{
public:
    XorInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb = nullptr);
    void output() const;
    void genMachineCode(AsmBuilder *);
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse() { return {operands[1]}; }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[1] == old)
        {
            operands[1]->removeUse(this);
            operands[1] = rep;
            rep->addUse(this);
        }
    }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }
};

class ZextInstruction : public Instruction // bool转为int
{
public:
    ZextInstruction(Operand *dst, Operand *src, bool b2i = false, BasicBlock *insert_bb = nullptr);
    void output() const;
    void genMachineCode(AsmBuilder *);
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse() { return {operands[1]}; }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[1] == old)
        {
            operands[1]->removeUse(this);
            operands[1] = rep;
            rep->addUse(this);
        }
    }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }

private:
    bool b2i;
};

class GepInstruction : public Instruction
{
public:
    GepInstruction(Operand *dst, Operand *base, std::vector<Operand *> offs, BasicBlock *insert_bb = nullptr, bool type2 = false);
    void output() const;
    void genMachineCode(AsmBuilder *);
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse()
    {
        std::vector<Operand *> vec;
        for (auto it = operands.begin() + 1; it != operands.end(); it++)
            vec.push_back(*it);
        return vec;
    }
    void replaceDef(Operand *rep)
    {
        if (operands[0])
        {
            operands[0]->setDef(nullptr);
            operands[0] = rep;
            operands[0]->setDef(this);
        }
    }
    void replaceUse(Operand *old, Operand *rep)
    {
        for (size_t i = 1; i < operands.size(); i++)
        {
            if (operands[i] == old)
            {
                operands[i]->removeUse(this);
                operands[i] = rep;
                rep->addUse(this);
            }
        }
    }

private:
    bool type2 = false;
};

class F2IInstruction : public Instruction
{
public:
    F2IInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb = nullptr);
    void output() const;
    void genMachineCode(AsmBuilder *);
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse() { return {operands[1]}; }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[1] == old)
        {
            operands[1]->removeUse(this);
            operands[1] = rep;
            rep->addUse(this);
        }
    }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }
};

class I2FInstruction : public Instruction
{
public:
    I2FInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb = nullptr);
    void output() const;
    void genMachineCode(AsmBuilder *);
    Operand *getDef()
    {
        return operands[0];
    }
    std::vector<Operand *> getUse() { return {operands[1]}; }
    void replaceUse(Operand *old, Operand *rep)
    {
        if (operands[1] == old)
        {
            operands[1]->removeUse(this);
            operands[1] = rep;
            rep->addUse(this);
        }
    }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }
};

class PhiInstruction : public Instruction
{
private:
    std::map<BasicBlock *, Operand *> srcs;
    Operand *addr; // old PTR

public:
    PhiInstruction(Operand *dst, BasicBlock *insert_bb = nullptr);
    ~PhiInstruction();
    void output() const;
    void addEdge(BasicBlock *block, Operand *src);
    Operand *getAddr() { return addr; };
    std::map<BasicBlock *, Operand *> &getSrcs() { return srcs; };

    void genMachineCode(AsmBuilder *)
    {
    }
    Operand *getDef() { return operands[0]; }
    std::vector<Operand *> getUse()
    {
        std::vector<Operand *> vec;
        for (auto &ope : operands)
            if (ope != operands[0])
                vec.push_back(ope);
        return vec;
    }
    void replaceUse(Operand *old, Operand *rep)
    {
        for (auto &it : srcs)
        {
            if (it.second == old)
            {
                it.second->removeUse(this);
                it.second = rep;
                rep->addUse(this);
            }
        }
        for (auto it = operands.begin() + 1; it != operands.end(); it++)
            if (*it == old)
                *it = rep;
    }
    void replaceDef(Operand *rep)
    {
        operands[0]->setDef(nullptr);
        operands[0] = rep;
        operands[0]->setDef(this);
    }
};

#endif

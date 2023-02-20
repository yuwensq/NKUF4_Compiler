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
        SITFP
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
};

class StoreInstruction : public Instruction
{
public:
    StoreInstruction(Operand *dst_addr, Operand *src, BasicBlock *insert_bb = nullptr);
    ~StoreInstruction();
    void output() const;
    void genMachineCode(AsmBuilder *);
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
};

class XorInstruction : public Instruction // not指令
{
public:
    XorInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb = nullptr);
    void output() const;
    void genMachineCode(AsmBuilder *);
};

class ZextInstruction : public Instruction // bool转为int
{
public:
    ZextInstruction(Operand *dst, Operand *src, bool b2i = false, BasicBlock *insert_bb = nullptr);
    void output() const;
    void genMachineCode(AsmBuilder *);

private:
    bool b2i;
};

class GepInstruction : public Instruction
{
public:
    GepInstruction(Operand *dst, Operand *base, std::vector<Operand *> offs, BasicBlock *insert_bb = nullptr, bool type2 = false);
    void output() const;
    void genMachineCode(AsmBuilder *);

private:
    bool type2 = false;
};

class F2IInstruction : public Instruction
{
public:
    F2IInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb = nullptr);
    void output() const;
    void genMachineCode(AsmBuilder *);
};

class I2FInstruction : public Instruction
{
public:
    I2FInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb = nullptr);
    void output() const;
    void genMachineCode(AsmBuilder *);
};

#endif
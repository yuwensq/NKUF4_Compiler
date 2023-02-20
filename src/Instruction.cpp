#include "Instruction.h"
#include "BasicBlock.h"
#include <iostream>
#include <cmath>
#include <assert.h>
#include <string>
#include "Function.h"
#include "Type.h"
#include "MachineCode.h"
extern FILE *yyout;

Instruction::Instruction(unsigned instType, BasicBlock *insert_bb)
{
    prev = next = this;
    opcode = -1;
    this->instType = instType;
    if (insert_bb != nullptr)
    {
        insert_bb->insertBack(this);
        parent = insert_bb;
    }
}

Instruction::~Instruction()
{
    parent->remove(this);
}

BasicBlock *Instruction::getParent()
{
    return parent;
}

void Instruction::setParent(BasicBlock *bb)
{
    parent = bb;
}

void Instruction::setNext(Instruction *inst)
{
    next = inst;
}

void Instruction::setPrev(Instruction *inst)
{
    prev = inst;
}

Instruction *Instruction::getNext()
{
    return next;
}

Instruction *Instruction::getPrev()
{
    return prev;
}

AllocaInstruction::AllocaInstruction(Operand *dst, SymbolEntry *se, BasicBlock *insert_bb) : Instruction(ALLOCA, insert_bb)
{
    operands.push_back(dst);
    dst->setDef(this);
    this->se = se;
}

AllocaInstruction::~AllocaInstruction()
{
    operands[0]->setDef(nullptr);
    if (operands[0]->usersNum() == 0)
        delete operands[0];
}

void AllocaInstruction::output() const
{
    std::string dst, type;
    dst = operands[0]->toStr();
    type = se->getType()->toStr();
    fprintf(yyout, "  %s = alloca %s, align 4\n", dst.c_str(), type.c_str());
}

LoadInstruction::LoadInstruction(Operand *dst, Operand *src_addr, BasicBlock *insert_bb) : Instruction(LOAD, insert_bb)
{
    operands.push_back(dst);
    operands.push_back(src_addr);
    dst->setDef(this);
    src_addr->addUse(this);
}

LoadInstruction::~LoadInstruction()
{
    operands[0]->setDef(nullptr);
    if (operands[0]->usersNum() == 0)
        delete operands[0];
    operands[1]->removeUse(this);
}

void LoadInstruction::output() const
{
    std::string dst = operands[0]->toStr();
    std::string src = operands[1]->toStr();
    std::string src_type;
    std::string dst_type;
    dst_type = operands[0]->getType()->toStr();
    src_type = operands[1]->getType()->toStr();
    fprintf(yyout, "  %s = load %s, %s %s, align 4\n", dst.c_str(), dst_type.c_str(), src_type.c_str(), src.c_str());
}

StoreInstruction::StoreInstruction(Operand *dst_addr, Operand *src, BasicBlock *insert_bb) : Instruction(STORE, insert_bb)
{
    operands.push_back(dst_addr);
    operands.push_back(src);
    dst_addr->addUse(this);
    src->addUse(this);
}

StoreInstruction::~StoreInstruction()
{
    operands[0]->removeUse(this);
    operands[1]->removeUse(this);
}

void StoreInstruction::output() const
{
    std::string dst = operands[0]->toStr();
    std::string src = operands[1]->toStr();
    std::string dst_type = operands[0]->getType()->toStr();
    std::string src_type = operands[1]->getType()->toStr();

    fprintf(yyout, "  store %s %s, %s %s, align 4\n", src_type.c_str(), src.c_str(), dst_type.c_str(), dst.c_str());
}

BinaryInstruction::BinaryInstruction(unsigned opcode, Operand *dst, Operand *src1, Operand *src2, BasicBlock *insert_bb) : Instruction(BINARY, insert_bb)
{
    this->opcode = opcode;
    operands.push_back(dst);
    operands.push_back(src1);
    operands.push_back(src2);
    dst->setDef(this);
    src1->addUse(this);
    src2->addUse(this);
    floatVersion = (src1->getType()->isFloat() || src2->getType()->isFloat());
}

BinaryInstruction::~BinaryInstruction()
{
    operands[0]->setDef(nullptr);
    if (operands[0]->usersNum() == 0)
        delete operands[0];
    operands[1]->removeUse(this);
    operands[2]->removeUse(this);
}

void BinaryInstruction::output() const
{
    std::string dst, src1, src2, op, type;
    dst = operands[0]->toStr();
    src1 = operands[1]->toStr();
    src2 = operands[2]->toStr();
    type = operands[0]->getType()->toStr();
    switch (opcode)
    {
    case ADD:
        op = floatVersion ? "fadd" : "add";
        break;
    case SUB:
        op = floatVersion ? "fsub" : "sub";
        break;
    case MUL:
        op = floatVersion ? "fmul" : "mul";
        break;
    case DIV:
        op = floatVersion ? "fdiv" : "sdiv";
        break;
    case MOD:
        op = "srem";
        break;
    default:
        break;
    }
    fprintf(yyout, "  %s = %s %s %s, %s\n", dst.c_str(), op.c_str(), type.c_str(), src1.c_str(), src2.c_str());
}

CmpInstruction::CmpInstruction(unsigned opcode, Operand *dst, Operand *src1, Operand *src2, BasicBlock *insert_bb) : Instruction(CMP, insert_bb)
{
    this->opcode = opcode;
    operands.push_back(dst);
    operands.push_back(src1);
    operands.push_back(src2);
    dst->setDef(this);
    src1->addUse(this);
    src2->addUse(this);
    floatVersion = (src1->getType()->isFloat() || src2->getType()->isFloat());
}

CmpInstruction::~CmpInstruction()
{
    operands[0]->setDef(nullptr);
    if (operands[0]->usersNum() == 0)
        delete operands[0];
    operands[1]->removeUse(this);
    operands[2]->removeUse(this);
}

void CmpInstruction::output() const
{
    std::string dst, src1, src2, op, type;
    dst = operands[0]->toStr();
    src1 = operands[1]->toStr();
    src2 = operands[2]->toStr();
    type = operands[1]->getType()->toStr();
    switch (opcode)
    {
    case E:
        op = floatVersion ? "oeq" : "eq";
        break;
    case NE:
        op = floatVersion ? "une" : "ne";
        break;
    case L:
        op = floatVersion ? "olt" : "slt";
        break;
    case LE:
        op = floatVersion ? "ole" : "sle";
        break;
    case G:
        op = floatVersion ? "ogt" : "sgt";
        break;
    case GE:
        op = floatVersion ? "oge" : "sge";
        break;
    default:
        op = "";
        break;
    }
    std::string cmp = floatVersion ? "fcmp" : "icmp";
    fprintf(yyout, "  %s = %s %s %s %s, %s\n", dst.c_str(), cmp.c_str(), op.c_str(), type.c_str(), src1.c_str(), src2.c_str());
}

UncondBrInstruction::UncondBrInstruction(BasicBlock *to, BasicBlock *insert_bb) : Instruction(UNCOND, insert_bb)
{
    branch = to;
}

void UncondBrInstruction::output() const
{
    fprintf(yyout, "  br label %%B%d\n", branch->getNo());
}

void UncondBrInstruction::setBranch(BasicBlock *bb)
{
    branch = bb;
}

BasicBlock *UncondBrInstruction::getBranch()
{
    return branch;
}

CondBrInstruction::CondBrInstruction(BasicBlock *true_branch, BasicBlock *false_branch, Operand *cond, BasicBlock *insert_bb) : Instruction(COND, insert_bb)
{
    this->true_branch = true_branch;
    this->false_branch = false_branch;
    cond->addUse(this);
    operands.push_back(cond);
}

CondBrInstruction::~CondBrInstruction()
{
    operands[0]->removeUse(this);
}

void CondBrInstruction::output() const
{
    std::string cond, type;
    cond = operands[0]->toStr();
    type = operands[0]->getType()->toStr();
    int true_label = true_branch->getNo();
    int false_label = false_branch->getNo();
    fprintf(yyout, "  br %s %s, label %%B%d, label %%B%d\n", type.c_str(), cond.c_str(), true_label, false_label);
}

void CondBrInstruction::setFalseBranch(BasicBlock *bb)
{
    false_branch = bb;
}

BasicBlock *CondBrInstruction::getFalseBranch()
{
    return false_branch;
}

void CondBrInstruction::setTrueBranch(BasicBlock *bb)
{
    true_branch = bb;
}

BasicBlock *CondBrInstruction::getTrueBranch()
{
    return true_branch;
}

CallInstruction::CallInstruction(Operand *dst, SymbolEntry *func, std::vector<Operand *> params, BasicBlock *insert_bb) : Instruction(CALL, insert_bb)
{
    operands.push_back(dst);
    if (dst != nullptr)
    {
        dst->setDef(this);
    }
    for (auto operand : params)
    {
        operands.push_back(operand);
        operand->addUse(this);
    }
    this->func = func;
}

CallInstruction::~CallInstruction() {}

void CallInstruction::output() const
{
    fprintf(yyout, "  ");
    if (operands[0] != nullptr)
    {
        fprintf(yyout, "%s = ", operands[0]->toStr().c_str());
    }
    fprintf(yyout, "call %s %s(", ((FunctionType *)(func->getType()))->getRetType()->toStr().c_str(), func->toStr().c_str());
    for (long unsigned int i = 1; i < operands.size(); i++)
    {
        if (i != 1)
        {
            fprintf(yyout, ", ");
        }
        fprintf(yyout, "%s %s", operands[i]->getType()->toStr().c_str(), operands[i]->toStr().c_str());
    }
    fprintf(yyout, ")\n");
}

RetInstruction::RetInstruction(Operand *src, BasicBlock *insert_bb) : Instruction(RET, insert_bb)
{
    if (src != nullptr)
    {
        operands.push_back(src);
        src->addUse(this);
    }
}

RetInstruction::~RetInstruction()
{
    if (!operands.empty())
        operands[0]->removeUse(this);
}

void RetInstruction::output() const
{
    if (operands.empty())
    {
        fprintf(yyout, "  ret void\n");
    }
    else
    {
        std::string ret, type;
        ret = operands[0]->toStr();
        type = operands[0]->getType()->toStr();
        fprintf(yyout, "  ret %s %s\n", type.c_str(), ret.c_str());
    }
}

XorInstruction::XorInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb) : Instruction(XOR, insert_bb)
{
    dst->setDef(this);
    src->addUse(this);
    operands.push_back(dst);
    operands.push_back(src);
}

void XorInstruction::output() const
{
    std::string dst, src;
    dst = operands[0]->toStr();
    src = operands[1]->toStr();
    fprintf(yyout, "  %s = xor i1 %s, true\n", dst.c_str(), src.c_str());
}

ZextInstruction::ZextInstruction(Operand *dst, Operand *src, bool b2i, BasicBlock *insert_bb) : Instruction(ZEXT, insert_bb)
{
    this->b2i = b2i;
    dst->setDef(this);
    src->addUse(this);
    operands.push_back(dst);
    operands.push_back(src);
}

void ZextInstruction::output() const
{
    std::string dst, src;
    dst = operands[0]->toStr();
    src = operands[1]->toStr();
    if (b2i)
    {
        fprintf(yyout, "  %s = zext i1 %s to i32\n", dst.c_str(), src.c_str());
    }
    else
    {

        fprintf(yyout, "  %s = zext i32 %s to i1\n", dst.c_str(), src.c_str());
    }
}
MachineOperand *Instruction::genMachineOperand(Operand *ope, AsmBuilder *builder = nullptr)
{
    auto se = ope->getEntry();
    MachineOperand *mope = nullptr;
    if (se->isConstant())
    {
        // 这里，如果一个立即数是浮点数，我们把它当成一个无符号32位数就行。
        if (se->getType()->isFloat())
        {
            float value = (float)dynamic_cast<ConstantSymbolEntry *>(se)->getValue();
            uint32_t v = reinterpret_cast<uint32_t &>(value);
            mope = new MachineOperand(MachineOperand::IMM, v);
        }
        else
            mope = new MachineOperand(MachineOperand::IMM, dynamic_cast<ConstantSymbolEntry *>(se)->getValue());
    }
    else if (se->isTemporary())
    {
        // 这个一会再管
        if (((TemporarySymbolEntry *)se)->isParam() && builder)
        {
            int argNum = dynamic_cast<TemporarySymbolEntry *>(se)->getArgNum();
            if (se->getType()->isFloat())
            {
                // https://developer.arm.com/documentation/den0018/a/Compiling-NEON-Instructions/NEON-assembler-and-ABI-restrictions/Passing-arguments-in-NEON-and-floating-point-registers?lang=en
                if (argNum < 16 && argNum >= 0)
                {
                    mope = new MachineOperand(MachineOperand::REG, argNum, true);
                }
                else
                { // 要从栈里加载
                    mope = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel(), true);
                    auto cur_block = builder->getBlock();
                    auto cur_inst = new LoadMInstruction(cur_block, LoadMInstruction::VLDR, new MachineOperand(*mope), new MachineOperand(MachineOperand::REG, 11), new MachineOperand(MachineOperand::IMM, 4 * -(argNum + 1)));
                    cur_block->InsertInst(cur_inst);
                    cur_block->addUInst(cur_inst);
                }
            }
            else
            {
                if (argNum < 4 && argNum >= 0)
                {
                    mope = new MachineOperand(MachineOperand::REG, argNum);
                }
                else
                { // 要从栈里加载
                    mope = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    auto cur_block = builder->getBlock();
                    auto cur_inst = new LoadMInstruction(cur_block, LoadMInstruction::LDR, new MachineOperand(*mope), new MachineOperand(MachineOperand::REG, 11), new MachineOperand(MachineOperand::IMM, 4 * -(argNum + 1)));
                    cur_block->InsertInst(cur_inst);
                    cur_block->addUInst(cur_inst);
                }
            }
        }
        else
        {
            if (se->getType()->isFloat())
                mope = new MachineOperand(MachineOperand::VREG, dynamic_cast<TemporarySymbolEntry *>(se)->getLabel(), true);
            else
                mope = new MachineOperand(MachineOperand::VREG, dynamic_cast<TemporarySymbolEntry *>(se)->getLabel());
        }
    }
    else if (se->isVariable())
    {
        auto id_se = dynamic_cast<IdentifierSymbolEntry *>(se);
        if (id_se->isGlobal())
            mope = new MachineOperand(id_se->toStr().c_str() + 1);
        else
            exit(0);
    }
    return mope;
}

MachineOperand *Instruction::genMachineReg(int reg, bool fpu = false)
{
    return new MachineOperand(MachineOperand::REG, reg, fpu);
}

MachineOperand *Instruction::genMachineVReg(bool fpu = false)
{
    return new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel(), fpu);
}

MachineOperand *Instruction::genMachineImm(int val)
{
    return new MachineOperand(MachineOperand::IMM, val);
}

MachineOperand *Instruction::genMachineLabel(int block_no)
{
    std::ostringstream buf;
    buf << ".L" << block_no;
    std::string label = buf.str();
    return new MachineOperand(label);
}

MachineOperand *Instruction::immToVReg(MachineOperand *imm, MachineBlock *cur_block)
{
    assert(imm->isImm());
    int value = imm->getVal();
    auto internal_reg = genMachineVReg();
    if (AsmBuilder::isLegalImm(value))
    {
        auto cur_inst = new MovMInstruction(cur_block, MovMInstruction::MOV, internal_reg, imm);
        cur_block->InsertInst(cur_inst);
    }
    else
    {
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, internal_reg, genMachineImm(value & 0xffff)));
        if (value & 0xff0000)
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg, internal_reg, genMachineImm(value & 0xff0000)));
        if (value & 0xff000000)
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg, internal_reg, genMachineImm(value & 0xff000000)));
    }
    return internal_reg;
}

void AllocaInstruction::genMachineCode(AsmBuilder *builder)
{
    /* HINT:
     * Allocate stack space for local variabel
     * Store frame offset in symbol entry */
    auto cur_func = builder->getFunction();
    int offset = cur_func->AllocSpace(se->getType()->getSize() / TypeSystem::intType->getSize() * 4);
    dynamic_cast<TemporarySymbolEntry *>(operands[0]->getEntry())->setOffset(-offset);
}

void LoadInstruction::genMachineCode(AsmBuilder *builder)
{
    auto cur_block = builder->getBlock();
    MachineInstruction *cur_inst = nullptr;
    bool floatVersion = operands[0]->getType()->isFloat();
    int ldrOp = floatVersion ? LoadMInstruction::VLDR : LoadMInstruction::LDR;
    // Load global operand
    if (operands[1]->getEntry()->isVariable() && dynamic_cast<IdentifierSymbolEntry *>(operands[1]->getEntry())->isGlobal())
    {
        auto dst = genMachineOperand(operands[0]);
        auto internal_reg1 = genMachineVReg();
        auto internal_reg2 = new MachineOperand(*internal_reg1);
        auto src = genMachineOperand(operands[1]);
        // example: load r0, addr_a
        cur_inst = new LoadMInstruction(cur_block, LoadMInstruction::LDR, internal_reg1, src);
        cur_block->InsertInst(cur_inst);
        // example: load r1, [r0]
        cur_inst = new LoadMInstruction(cur_block, ldrOp, dst, internal_reg2);
        cur_block->InsertInst(cur_inst);
    }
    // Load local operand
    else if (operands[1]->getEntry()->isTemporary() && operands[1]->getDef() && operands[1]->getDef()->isAlloc())
    {
        // example: load r1, [r0, #4]
        auto dst = genMachineOperand(operands[0]);
        auto src1 = genMachineReg(11);
        int offset = dynamic_cast<TemporarySymbolEntry *>(operands[1]->getEntry())->getOffset();
        if (AsmBuilder::isLegalImm(offset) || offset > -255) // 是合法立即数
        {
            cur_inst = new LoadMInstruction(cur_block, ldrOp, dst, src1, genMachineImm(offset));
            cur_block->InsertInst(cur_inst);
        }
        else
        {
            // 低16位用mov，高16位用两个add
            auto internal_reg = genMachineVReg();
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, internal_reg, genMachineImm(offset & 0xffff)));
            if (offset & 0xff0000)
                cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg, internal_reg, genMachineImm(offset & 0xff0000)));
            if (offset & 0xff000000)
                cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg, internal_reg, genMachineImm(offset & 0xff000000)));
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg, internal_reg, src1));
            cur_block->InsertInst(new LoadMInstruction(cur_block, ldrOp, dst, internal_reg));
        }
    }
    // Load operand from temporary variable
    else
    {
        // example: load r1, [r0]
        auto dst = genMachineOperand(operands[0]);
        auto src = genMachineOperand(operands[1]);
        cur_inst = new LoadMInstruction(cur_block, ldrOp, dst, src);
        cur_block->InsertInst(cur_inst);
    }
}

void StoreInstruction::genMachineCode(AsmBuilder *builder)
{
    // TODO
    auto cur_block = builder->getBlock();
    MachineInstruction *cur_inst = nullptr;
    auto src = genMachineOperand(operands[1], builder);
    bool floatVersion = operands[1]->getType()->isFloat();
    int strOp = floatVersion ? StoreMInstruction::VSTR : StoreMInstruction::STR;
    if (src->isImm()) // 这里立即数可能为浮点数，这样做也没问题
    {
        src = new MachineOperand(*immToVReg(src, cur_block));
        if (floatVersion)
        {
            auto internal_reg = genMachineVReg(true);
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, src));
            src = new MachineOperand(*internal_reg);
        }
    }
    // Store global operand
    if (operands[0]->getEntry()->isVariable() && dynamic_cast<IdentifierSymbolEntry *>(operands[0]->getEntry())->isGlobal())
    {
        auto dst = genMachineOperand(operands[0]);
        auto internal_reg1 = genMachineVReg();
        auto internal_reg2 = new MachineOperand(*internal_reg1);
        // example: load r0, addr_a
        cur_inst = new LoadMInstruction(cur_block, LoadMInstruction::LDR, internal_reg1, dst);
        cur_block->InsertInst(cur_inst);
        // example: store r1, [r0]
        cur_inst = new StoreMInstruction(cur_block, strOp, src, internal_reg2);
        cur_block->InsertInst(cur_inst);
    }
    // Store local operand
    else if (operands[0]->getEntry()->isTemporary() && operands[0]->getDef() && operands[0]->getDef()->isAlloc())
    {
        // example: store r1, [r0, #4]
        auto dst = genMachineReg(11);
        // auto off = genMachineImm(dynamic_cast<TemporarySymbolEntry *>(operands[0]->getEntry())->getOffset());
        // cur_inst = new StoreMInstruction(cur_block, src, dst, off);
        // cur_block->InsertInst(cur_inst);
        int offset = dynamic_cast<TemporarySymbolEntry *>(operands[0]->getEntry())->getOffset();
        if (AsmBuilder::isLegalImm(offset) || offset > -255)
        {
            cur_inst = new StoreMInstruction(cur_block, strOp, src, dst, genMachineImm(offset));
            cur_block->InsertInst(cur_inst);
        }
        else
        {
            auto internal_reg = genMachineVReg();
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, internal_reg, genMachineImm(offset & 0xffff)));
            if (offset & 0xff0000)
                cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg, internal_reg, genMachineImm(offset & 0xff0000)));
            if (offset & 0xff000000)
                cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg, internal_reg, genMachineImm(offset & 0xff000000)));
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg, internal_reg, dst));
            cur_block->InsertInst(new StoreMInstruction(cur_block, strOp, src, internal_reg));
        }
    }
    // Load operand from temporary variable
    else
    {
        // example: store r1, [r0]
        auto dst = genMachineOperand(operands[0]);
        cur_inst = new StoreMInstruction(cur_block, strOp, src, dst);
        cur_block->InsertInst(cur_inst);
    }
}

void BinaryInstruction::genMachineCode(AsmBuilder *builder)
{
    // TODO:
    // complete other instructions
    // 这个函数要改改，软浮点要调用一些ABI
    auto cur_block = builder->getBlock();
    auto dst = genMachineOperand(operands[0]);
    auto src1 = genMachineOperand(operands[1]);
    auto src2 = genMachineOperand(operands[2]);
    /* HINT:
     * The source operands of ADD instruction in ir code both can be immediate num.
     * However, it's not allowed in assembly code.
     * So you need to insert LOAD/MOV instrucrion to load immediate num into register.
     * As to other instructions, such as MUL, CMP, you need to deal with this situation, too.*/
    MachineInstruction *cur_inst = nullptr;
    if (src1->isImm())
    {
        src1 = new MachineOperand(*immToVReg(src1, cur_block));
        if (floatVersion)
        {
            auto internal_reg = genMachineVReg(true);
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, src1));
            src1 = new MachineOperand(*internal_reg);
        }
    }
    if (src2->isImm())
    {
        if (floatVersion) // 如果是浮点数，直接放寄存器里得了
        {
            src2 = new MachineOperand(*immToVReg(src2, cur_block));
            auto internal_reg = genMachineVReg(true);
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, src2));
            src2 = new MachineOperand(*internal_reg);
        }
        else if (opcode == MUL || opcode == DIV || opcode == MOD || !AsmBuilder::isLegalImm(src2->getVal()))
        {
            // int类型，按需放寄存器里
            src2 = new MachineOperand(*immToVReg(src2, cur_block));
        }
    }
    switch (opcode)
    {
    case ADD:
        cur_inst = new BinaryMInstruction(cur_block,
                                          floatVersion ? BinaryMInstruction::VADD : BinaryMInstruction::ADD,
                                          dst,
                                          src1,
                                          src2);
        break;
    case SUB:
        cur_inst = new BinaryMInstruction(cur_block,
                                          floatVersion ? BinaryMInstruction::VSUB : BinaryMInstruction::SUB,
                                          dst,
                                          src1,
                                          src2);
        break;
    case MUL:
        cur_inst = new BinaryMInstruction(cur_block,
                                          floatVersion ? BinaryMInstruction::VMUL : BinaryMInstruction::MUL,
                                          dst,
                                          src1,
                                          src2);
        break;
    case DIV:
        cur_inst = new BinaryMInstruction(cur_block,
                                          floatVersion ? BinaryMInstruction::VDIV : BinaryMInstruction::DIV,
                                          dst,
                                          src1,
                                          src2);
        break;
    case AND: // 下边这俩，操作数不会是浮点数，因为已经被隐式转化了
        cur_inst = new BinaryMInstruction(cur_block, BinaryMInstruction::AND, dst, src1, src2);
        break;
    case OR:
        cur_inst = new BinaryMInstruction(cur_block, BinaryMInstruction::OR, dst, src1, src2);
        break;
    case MOD:
        // arm里没有模指令，要除乘减结合，来算余数
        {
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::DIV, dst, src1, src2));
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::MUL, new MachineOperand(*dst), new MachineOperand(*dst), new MachineOperand(*src2)));
            cur_inst = new BinaryMInstruction(cur_block, BinaryMInstruction::SUB, new MachineOperand(*dst), new MachineOperand(*src1), new MachineOperand(*dst));
        }
        break;
    default:
        break;
    }
    cur_block->InsertInst(cur_inst);
}

void CmpInstruction::genMachineCode(AsmBuilder *builder)
{
    // TODO
    // 简简单单生成一条cmp指令就行
    // 加了浮点数，这里也要改
    auto cur_block = builder->getBlock();
    auto src1 = genMachineOperand(operands[1]);
    auto src2 = genMachineOperand(operands[2]);
    if (src1->isImm())
    {
        src1 = new MachineOperand(*immToVReg(src1, cur_block));
        if (floatVersion)
        {
            auto internal_reg = genMachineVReg(true);
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, src1));
            src1 = new MachineOperand(*internal_reg);
        }
    }
    if (src2->isImm())
    {
        src2 = new MachineOperand(*immToVReg(src2, cur_block));
        if (floatVersion)
        {
            auto internal_reg = genMachineVReg(true);
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, src2));
            src2 = new MachineOperand(*internal_reg);
        }
    }
    if (floatVersion)
    {
        cur_block->InsertInst(new CmpMInstruction(cur_block, CmpMInstruction::VCMP, src1, src2));
        cur_block->InsertInst(new VmrsMInstruction(cur_block));
    }
    else
    { // 不是浮点的，直接生成普通cmp就行
        cur_block->InsertInst(new CmpMInstruction(cur_block, CmpMInstruction::CMP, src1, src2));
    }
    // 这里借助builder向br指令传cond
    int cmpOpCode = 0, minusOpCode = 0;
    switch (opcode)
    {
    case E:
    {
        cmpOpCode = CmpMInstruction::EQ;
        minusOpCode = CmpMInstruction::NE;
    }
    break;
    case NE:
    {
        cmpOpCode = CmpMInstruction::NE;
        minusOpCode = CmpMInstruction::EQ;
    }
    break;
    case L:
    {
        cmpOpCode = CmpMInstruction::LT;
        minusOpCode = CmpMInstruction::GE;
    }
    break;
    case LE:
    {
        cmpOpCode = CmpMInstruction::LE;
        minusOpCode = CmpMInstruction::GT;
    }
    break;
    case G:
    {
        cmpOpCode = CmpMInstruction::GT;
        minusOpCode = CmpMInstruction::LE;
    }
    break;
    case GE:
    {
        cmpOpCode = CmpMInstruction::GE;
        minusOpCode = CmpMInstruction::LT;
    }
    break;
    default:
        break;
    }
    auto dst = genMachineOperand(operands[0]);
    cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, dst, genMachineImm(1), cmpOpCode));
    cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, dst, genMachineImm(0), minusOpCode));
    builder->setCmpOpcode(cmpOpCode);
}

void UncondBrInstruction::genMachineCode(AsmBuilder *builder)
{
    // TODO
    // 直接生成一条指令就行
    auto cur_block = builder->getBlock();
    cur_block->InsertInst(new BranchMInstruction(cur_block, BranchMInstruction::B, genMachineLabel(branch->getNo())));
}

void CondBrInstruction::genMachineCode(AsmBuilder *builder)
{
    // TODO
    // 生成两条指令
    auto cur_block = builder->getBlock();
    cur_block->InsertInst(new BranchMInstruction(cur_block, BranchMInstruction::B, genMachineLabel(true_branch->getNo()), builder->getCmpOpcode()));
    cur_block->InsertInst(new BranchMInstruction(cur_block, BranchMInstruction::B, genMachineLabel(false_branch->getNo())));
}

void CallInstruction::genMachineCode(AsmBuilder *builder)
{
    auto cur_block = builder->getBlock();
    // 先把不是浮点数的放到r0-r3里
    long unsigned int i;
    int sum = 0;
    for (i = 1; i <= operands.size() - 1 && sum < 4; i++)
    {
        if (operands[i]->getType()->isFloat())
            continue;
        auto param = genMachineOperand(operands[i]);
        // 一条mov解决不了
        if (param->isImm() && (param->getVal() & 0xffff0000))
        {
            param = new MachineOperand(*immToVReg(param, cur_block));
        }
        // 用mov指令把参数放到对应寄存器里
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, genMachineReg(sum), param));
        sum++;
    }
    int intLastPos = i;
    // 把浮点数放到寄存器里
    sum = 0;
    for (i = 1; i <= operands.size() - 1 && sum < 16; i++)
    {
        if (!operands[i]->getType()->isFloat())
            continue;
        auto param = genMachineOperand(operands[i]);
        if (param->isImm())
        {
            param = new MachineOperand(*immToVReg(param, cur_block));
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, genMachineReg(sum, true), param));
        }
        else
        {
            // 用mov指令把参数放到对应寄存器里
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV32, genMachineReg(sum, true), param));
        }
        sum++;
    }
    int floatLastPos = i;
    int param_size_in_stack = 0;
    // 开始从后向前push
    for (long unsigned int i = operands.size() - 1; i >= 1; i--)
    {
        if (operands[i]->getType()->isFloat() && i < floatLastPos)
            continue;
        if (!operands[i]->getType()->isFloat() && i < intLastPos)
            continue;
        auto param = genMachineOperand(operands[i]);
        if (param->isFReg())
        {
            cur_block->InsertInst(new StackMInstrcuton(cur_block, StackMInstrcuton::VPUSH, {param}));
        }
        else
        {
            if (param->isImm())
            {
                param = new MachineOperand(*immToVReg(param, cur_block));
            }
            cur_block->InsertInst(new StackMInstrcuton(cur_block, StackMInstrcuton::PUSH, {param}));
        }
        param_size_in_stack += 4;
    }
    // 生成bl指令，调用函数
    cur_block->InsertInst(new BranchMInstruction(cur_block, BranchMInstruction::BL, new MachineOperand(func->toStr().c_str())));
    // 生成add指令释放栈空间
    if (param_size_in_stack > 0)
    {
        auto sp = genMachineReg(13);
        auto stack_size = genMachineImm(param_size_in_stack);
        if (AsmBuilder::isLegalImm(param_size_in_stack))
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, sp, sp, stack_size));
        else
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, sp, sp, new MachineOperand(*immToVReg(stack_size, cur_block))));
    }
    if (operands[0])
    {
        if (operands[0]->getType()->isFloat())
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV32, genMachineOperand(operands[0]), genMachineReg(0, true)));
        else
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, genMachineOperand(operands[0]), genMachineReg(0)));
    }
}

void RetInstruction::genMachineCode(AsmBuilder *builder)
{
    // TODO
    /* HINT:
     * 1. Generate mov instruction to save return value in r0
     * 2. Restore callee saved registers and sp, fp
     * 3. Generate bx instruction */
    auto cur_bb = builder->getBlock();
    // 如果有返回值
    if (operands.size() > 0)
    {
        auto ret_value = genMachineOperand(operands[0]);
        if (ret_value->isImm())
            ret_value = new MachineOperand(*immToVReg(ret_value, cur_bb));
        if (operands[0]->getType()->isFloat())
        {
            if (ret_value->isFReg())
                cur_bb->InsertInst(new MovMInstruction(cur_bb, MovMInstruction::VMOV32, genMachineReg(0, true), ret_value));
            else // 同样的，这种情况是返回立即数，把立即数放到r寄存器里了
                cur_bb->InsertInst(new MovMInstruction(cur_bb, MovMInstruction::VMOV, genMachineReg(0, true), ret_value));
        }
        else
            cur_bb->InsertInst(new MovMInstruction(cur_bb, MovMInstruction::MOV, genMachineReg(0), ret_value));
    }
    auto sp = genMachineReg(13);
    // 释放栈空间，这里直接来一条mov就行了
    auto cur_inst = new MovMInstruction(cur_bb, MovMInstruction::MOV, sp, genMachineReg(11));
    cur_bb->InsertInst(cur_inst);
    // 恢复保存的寄存器，这里还不知道，先欠着
    auto curr_inst = new StackMInstrcuton(cur_bb, StackMInstrcuton::VPOP, {});
    cur_bb->InsertInst(curr_inst);
    cur_bb->addUInst(curr_inst);
    curr_inst = new StackMInstrcuton(cur_bb, StackMInstrcuton::POP, {});
    cur_bb->InsertInst(curr_inst);
    cur_bb->addUInst(curr_inst);
    // bx指令
    cur_bb->InsertInst(new BranchMInstruction(cur_bb, BranchMInstruction::BX, genMachineReg(14)));
}

void XorInstruction::genMachineCode(AsmBuilder *builder)
{
    auto cur_block = builder->getBlock();
    auto dst = genMachineOperand(operands[0]);
    auto src = genMachineOperand(operands[1]);
    cur_block->InsertInst(new CmpMInstruction(cur_block, CmpMInstruction::CMP, src, genMachineImm(0)));
    cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, dst, genMachineImm(1), CmpMInstruction::EQ));
    cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, dst, genMachineImm(0), CmpMInstruction::NE));
    builder->setCmpOpcode(CmpMInstruction::EQ);
}

void ZextInstruction::genMachineCode(AsmBuilder *builder)
{
    // 生成一条mov指令得了
    auto cur_block = builder->getBlock();
    auto dst = genMachineOperand(operands[0]);
    auto src = genMachineOperand(operands[1]);
    cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, dst, src));
}

void GepInstruction::genMachineCode(AsmBuilder *builder)
{
    // type2表示是不是通过传参传过来的数组指针，为true表示是，否则表示局部变量或者全局变量
    auto cur_block = builder->getBlock();
    auto dst = genMachineOperand(operands[0]);
    // 这里就是对于局部变量或者全局变量，要先把它们地址放到一个临时寄存器里，
    // 而函数参数，其实operand[1]就存的有地址
    auto base = type2 ? genMachineOperand(operands[1]) : genMachineVReg();
    // 全局变量，先load
    if (operands[1]->getEntry()->isVariable() && dynamic_cast<IdentifierSymbolEntry *>(operands[1]->getEntry())->isGlobal())
    {
        auto src = genMachineOperand(operands[1]);
        cur_block->InsertInst(new LoadMInstruction(cur_block, LoadMInstruction::LDR, base, src));
        base = new MachineOperand(*base);
    }
    else if (!type2) // 局部变量
    {
        // 偏移都是负数
        int offset = ((TemporarySymbolEntry *)operands[1]->getEntry())->getOffset();
        auto off = genMachineImm(offset);
        if (AsmBuilder::isLegalImm(offset) || offset > -255)
        {
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, base, genMachineReg(11), off));
            base = new MachineOperand(*base);
        }
        else
        {
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, base, genMachineReg(11), new MachineOperand(*immToVReg(off, cur_block))));
            base = new MachineOperand(*base);
        }
    }
    Type *arrType = ((PointerType *)operands[1]->getType())->getType();
    std::vector<int> indexs = ((ArrayType *)arrType)->getIndexs();
    std::vector<int> imms; // 这个专门用来记录索引中的立即数比如说a[10][i][3] 就存一个{0, 2}这样子
    for (unsigned long int i = 2; i < operands.size(); i++)
    {
        if (operands[i]->getEntry()->isConstant())
        {
            // 为了省代码，所有的立即数一起算，这里先跳过
            imms.push_back(i);
            continue;
        }
        unsigned int step = 4;
        for (unsigned long int j = i - (type2 ? 2 : 1); j < indexs.size(); j++)
        {
            step *= indexs[j];
        }
        auto off = genMachineVReg();
        cur_block->InsertInst(new LoadMInstruction(cur_block, LoadMInstruction::LDR, off, genMachineImm(step)));
        auto internal_reg1 = genMachineVReg();
        auto src1 = genMachineOperand(operands[i]);
        cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::MUL, internal_reg1, src1, off));
        auto internal_reg2 = genMachineVReg();
        cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg2, new MachineOperand(*base), new MachineOperand(*internal_reg1)));
        base = new MachineOperand(*internal_reg2);
    }
    int off = 0;
    for (auto index : imms)
    {
        int imm = ((ConstantSymbolEntry *)operands[index]->getEntry())->getValue();
        unsigned int step = 4;
        for (unsigned long int j = index - (type2 ? 2 : 1); j < indexs.size(); j++)
        {
            step *= indexs[j];
        }
        off += (imm * step);
    }
    if (off > 0)
    {
        auto internal_reg1 = genMachineImm(off);
        if (!AsmBuilder::isLegalImm(off))
        {
            internal_reg1 = new MachineOperand(*immToVReg(internal_reg1, cur_block));
        }
        cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, new MachineOperand(*base), new MachineOperand(*base), new MachineOperand(*internal_reg1)));
    }
    // for (unsigned long int i = 2; i < operands.size(); i++)
    // {
    //     unsigned int step = 4;
    //     for (unsigned long int j = i - (type2 ? 2 : 1); j < indexs.size(); j++)
    //     {
    //         step *= indexs[j];
    //     }
    //     auto off = genMachineVReg();
    //     cur_block->InsertInst(new LoadMInstruction(cur_block, off, genMachineImm(step)));
    //     auto internal_reg1 = genMachineVReg();
    //     auto src1 = genMachineOperand(operands[i]);
    //     if (src1->isImm())
    //     {
    //         auto internal_reg = genMachineVReg();
    //         cur_block->InsertInst(new LoadMInstruction(cur_block, internal_reg, src1));
    //         src1 = new MachineOperand(*internal_reg);
    //     }
    //     cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::MUL, internal_reg1, src1, off));
    //     auto internal_reg2 = genMachineVReg();
    //     cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, internal_reg2, new MachineOperand(*base), new MachineOperand(*internal_reg1)));
    //     base = new MachineOperand(*internal_reg2);
    // }
    cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, dst, base));
}

GepInstruction::GepInstruction(Operand *dst, Operand *base, std::vector<Operand *> offs, BasicBlock *insert_bb, bool type2) : Instruction(GEP, insert_bb), type2(type2)
{
    operands.push_back(dst);
    operands.push_back(base);
    dst->setDef(this);
    base->addUse(this);
    for (auto off : offs)
    {
        operands.push_back(off);
        off->addUse(this);
    }
}

void GepInstruction::output() const
{
    Operand *dst = operands[0];
    Operand *base = operands[1];
    std::string arrType = base->getType()->toStr();
    if (!type2)
    {
        fprintf(yyout, "  %s = getelementptr inbounds %s, %s %s, i32 0",
                dst->toStr().c_str(), arrType.substr(0, arrType.size() - 1).c_str(),
                arrType.c_str(), base->toStr().c_str());
    }
    else
    {
        fprintf(yyout, "  %s = getelementptr inbounds %s, %s %s",
                dst->toStr().c_str(), arrType.substr(0, arrType.size() - 1).c_str(),
                arrType.c_str(), base->toStr().c_str());
    }
    for (unsigned long int i = 2; i < operands.size(); i++)
    {
        fprintf(yyout, ", i32 %s", operands[i]->toStr().c_str());
    }
    fprintf(yyout, "\n");
}

F2IInstruction::F2IInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb) : Instruction(FPTSI, insert_bb)
{
    operands.push_back(dst);
    operands.push_back(src);
    dst->setDef(this);
    src->addUse(this);
}

void F2IInstruction::output() const
{
    std::string dst, src;
    dst = operands[0]->toStr();
    src = operands[1]->toStr();
    fprintf(yyout, "  %s = fptosi float %s to i32\n", dst.c_str(), src.c_str());
}

void F2IInstruction::genMachineCode(AsmBuilder *builder)
{ // 浮点转int
    auto cur_block = builder->getBlock();
    auto dst = genMachineOperand(operands[0]);
    auto src = genMachineOperand(operands[1]);
    if (src->isImm())
    { // 按理说立即数其实可以不用这条指令的，我们直接强制类型转化一下就行
        // 但是需要在生成语法树的时候做更多工作，这里偷个懒
        src = new MachineOperand(*immToVReg(src, cur_block));
    }
    if (src->isFReg())
    { // 如果src本来就是个浮点寄存器
        auto internal_reg = genMachineVReg(true);
        cur_block->InsertInst(new VcvtMInstruction(cur_block, VcvtMInstruction::FTS, internal_reg, src));
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, dst, internal_reg));
    }
    else
    {
        // 这种情况可能是浮点立即数转int
        auto internal_reg = genMachineVReg(true);
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, src));
        cur_block->InsertInst(new VcvtMInstruction(cur_block, VcvtMInstruction::FTS, internal_reg, internal_reg));
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, dst, internal_reg));
    }
}

I2FInstruction::I2FInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb) : Instruction(SITFP, insert_bb)
{
    operands.push_back(dst);
    operands.push_back(src);
    dst->setDef(this);
    src->addUse(this);
}

void I2FInstruction::output() const
{
    std::string dst, src;
    dst = operands[0]->toStr();
    src = operands[1]->toStr();
    fprintf(yyout, "  %s = sitofp i32 %s to float\n", dst.c_str(), src.c_str());
}

void I2FInstruction::genMachineCode(AsmBuilder *builder)
{
    auto cur_block = builder->getBlock();
    auto dst = genMachineOperand(operands[0]);
    auto src = genMachineOperand(operands[1]);
    if (src->isImm())
    {
        src = new MachineOperand(*immToVReg(src, cur_block));
    }
    assert(dst->isFReg());
    cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, dst, src));
    cur_block->InsertInst(new VcvtMInstruction(cur_block, VcvtMInstruction::STF, dst, dst));
}

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

#define genMachineOperand(name) genMOperand(name, builder)

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

bool Instruction::isCritical()
{
    if (isRet())
    {
        // 只有一个return;那么就是关键指令
        if (getUse().empty())
            return true;
        // 下面讨论return有返回值
        // 这边的preds取的应该是调用了当前函数的那些call指令
        auto callPreds = parent->getParent()->getCallPred();
        // 如果为空，那么关键
        if (callPreds.empty())
            return true;
        // 只要有接收ret值的就要返回true
        for (auto it : callPreds)
            if (it->getDef()->usersNum())
                return true;
        // 当前函数的return语句有返回值，当前的函数有被call指令调用，但这若干条call指令的结果都没有被使用，那么返回false
        return false;
    }
    // input/output
    // a function is essential if it is a sysy/memset function or it has a array param or it call a essential function or修改全局
    // 论文说：call指令都不能删，毕竟考虑到它可能修改全局变量这样，不过仔细设计倒也不是不行
    if (isCall())
    {
        IdentifierSymbolEntry *funcSE = (IdentifierSymbolEntry *)(((CallInstruction *)this)->getFunc());
        // 我们的那个代码里面用的llvm.memset.p0i8.i32，表示写内存
        // Sysy判断是否为库函数,memset需要提前到sysy判断之前，因为memset算sysy函数
        if (funcSE->getName() == "llvm.memset.p0i8.i32")
        {
            // 这边判断一下，如果memset的数组，它没有用过，那么这个memset也就不要了
            auto addr = getUse()[0];
            Instruction *bitDef = addr->getDef();
            if (bitDef->isBitcast())
            {
                // 获取了那个数组的基地址
                auto base = bitDef->getUse()[0];
                // 如果这个数组没有其他的use了，它就是一个无用初始化
                if (base->getUse().size() == 1)
                {
                    return false;
                }
            }
            return true;
        }
        else if (funcSE->isSysy())
        {
            return true;
        }
        else
        {
            // 讨论是否是纯函数->不是纯函数或者调用的函数有不是纯函数的话，就是关键函数
            auto func = funcSE->getFunction();
            if (func->getCritical() == 1)
            {
                return true;
            }
        }
        // if (funcSE->isSysy() || funcSE->getName() == "llvm.memset.p0i8.i32") {
        //     return true;
        // } else {
        //     //讨论是否是纯函数->不是纯函数或者调用的函数有不是纯函数的话，就是关键函数
        //     auto func = funcSE->getFunction();
        //     if (func->getCritical() == 1) {
        //         return true;
        //     }
        // }
    }
    // 涉及内存写,如果这条store语句所在的函数有被调用，且这个函数不是纯函数，就要保留它
    if (isStore())
    {
        auto func = parent->getParent();
        auto callPreds = func->getCallPred();
        // 如果为空，那么没有call调用这条store所在函数
        if (!callPreds.empty() && func->getCritical() == 1)
            return true;
        return false;
    }

    return false;
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
    if (branch == nullptr)
        Log("branch is null");
    fprintf(yyout, "  br label %%B%d\n", branch ? branch->getNo() : -1);
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
    this->isTailCall = false;
    this->funcAddPred();
}

void CallInstruction::funcAddPred()
{
    IdentifierSymbolEntry *funcSE = (IdentifierSymbolEntry *)func;
    if (!funcSE->isSysy() && funcSE->getName() != "llvm.memset.p0i8.i32")
    {
        funcSE->getFunction()->addCallPred(this);
    }
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
MachineOperand *Instruction::genMOperand(Operand *ope, AsmBuilder *builder = nullptr)
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
                    cur_block->getParent()->addUInst(cur_inst);
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
                    cur_block->getParent()->addUInst(cur_inst);
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
        auto cur_inst = new LoadMInstruction(cur_block, LoadMInstruction::LDR, internal_reg, imm);
        cur_block->InsertInst(cur_inst);
    }
    return internal_reg;
}

MachineOperand *Instruction::fimmToVReg(MachineOperand *imm, MachineBlock *cur_block, AsmBuilder *builder)
{
    assert(imm->isImm());
    auto internal_reg = genMachineVReg(true);
    // 可以用vmov32，就用vmov32
    if (builder->couldUseVMOV(imm->getVal()))
    {
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV32, internal_reg, imm));
    }
    else
    {
        imm = new MachineOperand(*immToVReg(imm, cur_block));
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, imm));
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
        cur_block->InsertInst(new LoadMInstruction(cur_block, ldrOp, dst, src1, genMachineImm(offset)));
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
    auto src = genMachineOperand(operands[1]);
    bool floatVersion = operands[1]->getType()->isFloat();
    int strOp = (floatVersion && !src->isImm()) ? StoreMInstruction::VSTR : StoreMInstruction::STR;
    if (src->isImm()) // 这里立即数可能为浮点数，这样做也没问题
    {
        src = new MachineOperand(*immToVReg(src, cur_block));
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
        int offset = dynamic_cast<TemporarySymbolEntry *>(operands[0]->getEntry())->getOffset();
        cur_inst = new StoreMInstruction(cur_block, strOp, src, dst, genMachineImm(offset));
        cur_block->InsertInst(cur_inst);
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
    auto cur_block = builder->getBlock();
    auto dst = genMachineOperand(operands[0]);
    auto src1 = genMachineOperand(operands[1]);
    auto src2 = genMachineOperand(operands[2]);
    MachineInstruction *cur_inst = nullptr;
    // 加法，这里交换一下
    if (opcode == BinaryInstruction::ADD && src1->isImm() && !src2->isImm())
        std::swap(src1, src2);
    if (src1->isImm())
    {
        if (floatVersion)
            src1 = new MachineOperand(*fimmToVReg(src1, cur_block, builder));
        else
            src1 = new MachineOperand(*immToVReg(src1, cur_block));
    }
    if (src2->isImm())
    {
        // 为零可以特殊处理
        if (src2->getVal() == 0) // 注意这里，因为浮点数0的位模式全零，也可以这样判断
        {
            if (opcode == ADD || opcode == SUB)
            {
                cur_block->InsertInst(new MovMInstruction(cur_block, floatVersion ? MovMInstruction::VMOV32 : MovMInstruction::MOV, dst, src1));
                return;
            }
            else if (opcode == MUL)
            {
                if (floatVersion)
                    cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::VSUB, dst, new MachineOperand(*dst), new MachineOperand(*dst)));
                else
                    cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, dst, genMachineImm(0)));
                return;
            }
        }
        if (src2->getVal() == 1 && (opcode == MUL || opcode == DIV) && !floatVersion)
        {
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, dst, src1));
            return;
        }
        if (floatVersion) // 如果是浮点数，直接放寄存器里得了
        {
            src2 = new MachineOperand(*fimmToVReg(src2, cur_block, builder));
            // src2 = new MachineOperand(*immToVReg(src2, cur_block));
            // auto internal_reg = genMachineVReg(true);
            // cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, src2));
            // src2 = new MachineOperand(*internal_reg);
        }
        // else if ((opcode == MUL) && (((int)src2->getVal() & ((int)src2->getVal() - 1)) == 0))
        // {
        //     int value = src2->getVal();
        //     int x = 0;
        //     while (value > 1)
        //     {
        //         value >>= 1;
        //         x++;
        //     }
        //     auto op = (opcode == MUL) ? BinaryMInstruction::LSL : BinaryMInstruction::ASR;
        //     cur_block->InsertInst(new BinaryMInstruction(cur_block, op, dst, src1, genMachineImm(x)));
        //     return;
        // }
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
    bool reverse = false;
    auto src1 = genMachineOperand(operands[1]);
    auto src2 = genMachineOperand(operands[2]);
    if (src1->isImm())
    {
        // 如果src1是立即数，src2不是，把他俩交换一下，然后判断码取反
        if (!floatVersion && AsmBuilder::isLegalImm(src1->getVal()) && !src2->isImm())
        {
            std::swap(src1, src2);
            reverse = true;
        }
        else
        {
            if (floatVersion)
                src1 = new MachineOperand(*fimmToVReg(src1, cur_block, builder));
            else
                src1 = new MachineOperand(*immToVReg(src1, cur_block));
        }
    }
    if (src2->isImm())
    {
        /*我是抗拒用goto的*/
        if (!floatVersion && AsmBuilder::isLegalImm(src2->getVal()))
            goto SKIP;
        if (floatVersion)
            src2 = new MachineOperand(*fimmToVReg(src2, cur_block, builder));
        else
            src2 = new MachineOperand(*immToVReg(src2, cur_block));
    }
SKIP:
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
        cmpOpCode = reverse ? CmpMInstruction::GT : CmpMInstruction::LT;
        minusOpCode = reverse ? CmpMInstruction::LE : CmpMInstruction::GE;
    }
    break;
    case LE:
    {
        cmpOpCode = reverse ? CmpMInstruction::GE : CmpMInstruction::LE;
        minusOpCode = reverse ? CmpMInstruction::LT : CmpMInstruction::GT;
    }
    break;
    case G:
    {
        cmpOpCode = reverse ? CmpMInstruction::LT : CmpMInstruction::GT;
        minusOpCode = reverse ? CmpMInstruction::GE : CmpMInstruction::LE;
    }
    break;
    case GE:
    {
        cmpOpCode = reverse ? CmpMInstruction::LE : CmpMInstruction::GE;
        minusOpCode = reverse ? CmpMInstruction::GT : CmpMInstruction::LT;
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

    auto funcSE = (IdentifierSymbolEntry *)(this->func);
    if (funcSE->getName() == "llvm.memset.p0i8.i32")
    {
        MachineOperand *operand;
        MachineInstruction *cur_inst;
        // std::cout << "llvm" << std::endl;
        auto r0 = genMachineReg(0);
        auto r1 = genMachineReg(1);
        auto r2 = genMachineReg(2);
        auto int8Ptr = operands[1];
        auto bitcast = (BitcastInstruction *)(int8Ptr->getDef());
        {
            auto arraySE = (TemporarySymbolEntry *)(bitcast->getSrc()->getEntry());
            int offset = arraySE->getOffset();
            operand = genMachineVReg();
            auto fp = genMachineReg(11);
            /*这里改成这样比较好*/
            if (AsmBuilder::isLegalImm(offset))
            {
                cur_block->InsertInst(new BinaryMInstruction(
                    cur_block, BinaryMInstruction::ADD, r0, fp, genMachineImm(offset)));
            }
            else
            {
                cur_inst = new LoadMInstruction(
                    cur_block, LoadMInstruction::LDR, operand, genMachineImm(offset));
                operand = new MachineOperand(*operand);
                cur_block->InsertInst(cur_inst);
                cur_block->InsertInst(new BinaryMInstruction(
                    cur_block, BinaryMInstruction::ADD, r0, fp, operand));
            }
        }
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, r1, genMachineImm(0)));
        auto len = genMachineOperand(operands[3]);
        /*这里也是改成这样比较好*/
        if (len->isImm() && !AsmBuilder::isLegalImm(len->getVal()))
        {
            operand = genMachineVReg();
            cur_inst = new LoadMInstruction(cur_block, LoadMInstruction::LDR, operand, len);
            operand = new MachineOperand(*operand);
            cur_block->InsertInst(cur_inst);
        }
        else
        {
            operand = len;
        }
        cur_block->InsertInst(new MovMInstruction(
            cur_block, MovMInstruction::MOV, r2, operand));
        cur_block->InsertInst(new BranchMInstruction(
            cur_block, BranchMInstruction::BL, new MachineOperand("@memset")));
        return;
    }
    // 先把不是浮点数的放到r0-r3里
    size_t i;
    int sum = 0;
    for (i = 1; i <= operands.size() - 1 && sum < 4; i++)
    {
        if (operands[i]->getType()->isFloat())
            continue;
        auto param = genMachineOperand(operands[i]);
        if (param->isImm())
        {
            if (!AsmBuilder::isLegalImm(param->getVal()))
                cur_block->InsertInst(new LoadMInstruction(cur_block, LoadMInstruction::LDR, genMachineReg(sum), param));
            else
                cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, genMachineReg(sum), param));
        }
        else
            cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::MOV, genMachineReg(sum), param));
        sum++;
    }
    auto intLastPos = i;
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
    auto floatLastPos = i;
    int param_size_in_stack = 0;
    // // 先看看一共需要push多少个
    // for (long unsigned int i = operands.size() - 1; i >= 1; i--)
    // {
    //     if (operands[i]->getType()->isFloat() && i < floatLastPos)
    //         continue;
    //     if (!operands[i]->getType()->isFloat() && i < intLastPos)
    //         continue;
    //     param_size_in_stack += 4;
    // }
    // 不是八字节对齐，额外push一个
    // bool needReduPush = ((param_size_in_stack % 8) != 0);
    // param_size_in_stack = needReduPush ? param_size_in_stack + 4 : param_size_in_stack;
    bool hasPushed = false;
    for (long unsigned int i = operands.size() - 1; i >= 1; i--)
    {
        if (operands[i]->getType()->isFloat() && i < floatLastPos)
            continue;
        if (!operands[i]->getType()->isFloat() && i < intLastPos)
            continue;
        hasPushed = true;
        auto param = genMachineOperand(operands[i]);
        if (param->isFReg())
        {
            cur_block->InsertInst(new StackMInstrcuton(cur_block, StackMInstrcuton::VPUSH, {param}));
            // if (needReduPush)
            // {
            //     needReduPush = false;
            //     cur_block->InsertInst(new StackMInstrcuton(cur_block, StackMInstrcuton::VPUSH, {new MachineOperand(*param)}));
            // }
        }
        else
        {
            if (param->isImm())
                param = new MachineOperand(*immToVReg(param, cur_block));
            cur_block->InsertInst(new StackMInstrcuton(cur_block, StackMInstrcuton::PUSH, {param}));
            // if (needReduPush)
            // {
            //     needReduPush = false;
            //     cur_block->InsertInst(new StackMInstrcuton(cur_block, StackMInstrcuton::PUSH, {new MachineOperand(*param)}));
            // }
        }
        param_size_in_stack += 4;
    }
    // 生成bl指令，调用函数
    auto blInst = new BranchMInstruction(cur_block, BranchMInstruction::BL, new MachineOperand(func->toStr().c_str()));
    // 这里如果用栈传递了参数就先不做尾调用了，还没想清楚
    blInst->setIsTailCall(!hasPushed && isTailCall);
    cur_block->InsertInst(blInst);
    // 生成add指令释放栈空间
    if (param_size_in_stack > 0)
    {
        auto sp = genMachineReg(13);
        auto stack_size = genMachineImm(param_size_in_stack);
        if (AsmBuilder::isLegalImm(param_size_in_stack))
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, sp, new MachineOperand(*sp), stack_size));
        else
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, sp, new MachineOperand(*sp), new MachineOperand(*immToVReg(stack_size, cur_block))));
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
    cur_bb->getParent()->addUInst(curr_inst);
    curr_inst = new StackMInstrcuton(cur_bb, StackMInstrcuton::POP, {});
    cur_bb->InsertInst(curr_inst);
    cur_bb->getParent()->addUInst(curr_inst);
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
        if (AsmBuilder::isLegalImm(offset))
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, base, genMachineReg(11), off));
        else
            cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, base, genMachineReg(11), new MachineOperand(*immToVReg(off, cur_block))));
        base = new MachineOperand(*base);
    }
    Type *arrType = ((PointerType *)operands[1]->getType())->getType();
    std::vector<int> indexs = {};
    if (arrType->isArray())
        indexs = ((ArrayType *)arrType)->getIndexs();
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
        cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::MUL, internal_reg1, src1, new MachineOperand(*off)));
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
        auto new_base = genMachineVReg();
        cur_block->InsertInst(new BinaryMInstruction(cur_block, BinaryMInstruction::ADD, new_base, new MachineOperand(*base), new MachineOperand(*internal_reg1)));
        base = new MachineOperand(*new_base);
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

BitcastInstruction::BitcastInstruction(Operand *dst, Operand *src, BasicBlock *insert_bb) : Instruction(BITCAST, insert_bb), dst(dst), src(src)
{
    operands.push_back(dst);
    operands.push_back(src);
    dst->setDef(this);
    src->addUse(this);
}

BitcastInstruction::~BitcastInstruction()
{
    operands[0]->setDef(nullptr);
    if (operands[0]->usersNum() == 0)
        delete operands[0];
    operands[1]->removeUse(this);
}

void BitcastInstruction::output() const
{
    std::string dst = operands[0]->toStr();
    std::string src = operands[1]->toStr();
    std::string dst_type = operands[0]->getType()->toStr();
    std::string src_type = operands[1]->getType()->toStr();
    fprintf(yyout, "  %s = bitcast %s %s to %s\n", dst.c_str(), src_type.c_str(), src.c_str(), dst_type.c_str());
}

void BitcastInstruction::genMachineCode(AsmBuilder *)
{
    //
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
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, dst, new MachineOperand(*internal_reg)));
    }
    else
    {
        // 这种情况可能是浮点立即数转int
        /*经过常量传播之后，这个分支已经不可能走到了*/
        auto internal_reg = genMachineVReg(true);
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, internal_reg, src));
        cur_block->InsertInst(new VcvtMInstruction(cur_block, VcvtMInstruction::FTS, new MachineOperand(*internal_reg), new MachineOperand(*internal_reg)));
        cur_block->InsertInst(new MovMInstruction(cur_block, MovMInstruction::VMOV, dst, new MachineOperand(*internal_reg)));
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
    cur_block->InsertInst(new VcvtMInstruction(cur_block, VcvtMInstruction::STF, new MachineOperand(*dst), new MachineOperand(*dst)));
}

PhiInstruction::PhiInstruction(Operand *dst, BasicBlock *insert_bb) : Instruction(PHI, insert_bb)
{
    operands.push_back(dst);
    // if (dst->getDef() == nullptr)
    //     dst->setDef(this);
    addr = dst;
}

PhiInstruction::~PhiInstruction()
{
    if (addr != nullptr)
    {
        // addr->setDef(nullptr);
        if (addr->usersNum() == 0)
            delete addr;
    }
}

void PhiInstruction::output() const
{
    fprintf(yyout, "  %s = phi %s ", operands[0]->toStr().c_str(), operands[0]->getType()->toStr().c_str());
    fprintf(stderr, "  %s = phi %s ", operands[0]->toStr().c_str(), operands[0]->getType()->toStr().c_str());
    if (srcs.empty())
    {
        fprintf(stderr, "\n");
        return;
    }
    auto it = srcs.begin();
    fprintf(yyout, "[ %s , %%B%d ]", it->second->toStr().c_str(), it->first->getNo());
    fprintf(stderr, "[ %s , %%B%d ]", it->second->toStr().c_str(), it->first->getNo());
    it++;
    for (; it != srcs.end(); it++)
    {
        fprintf(yyout, ", ");
        fprintf(stderr, ", ");
        fprintf(yyout, "[ %s , %%B%d ]", it->second->toStr().c_str(), it->first->getNo());
        fprintf(stderr, "[ %s , %%B%d ]", it->second->toStr().c_str(), it->first->getNo());
    }
    fprintf(yyout, "\n");
    fprintf(stderr, "\n");
}

void PhiInstruction::addEdge(BasicBlock *block, Operand *src)
{
    operands.push_back(src);
    srcs[block] = src;
    src->addUse(this);
}

bool PhiInstruction::findSrc(BasicBlock *block)
{
    for (auto it = srcs.begin(); it != srcs.end(); it++)
    {
        if (it->first == block)
        {
            return true;
        }
    }
    return false;
}

Operand *PhiInstruction::getBlockSrc(BasicBlock *block)
{
    if (srcs.find(block) != srcs.end())
        return srcs[block];
    return nullptr;
}

void PhiInstruction::removeBlockSrc(BasicBlock *block)
{
    for (auto it = srcs.begin(); it != srcs.end(); it++)
    {
        if (it->first == block)
        {
            // 使用erase时容器失效
            srcs.erase(block);
            removeUse(it->second);
            it->second->removeUse(this);
            return;
        }
    }
    return;
}

void PhiInstruction::addSrc(BasicBlock *block, Operand *src)
{
    operands.push_back(src);
    srcs.insert(std::make_pair(block, src));
    src->addUse(this);
}

void PhiInstruction::removeUse(Operand *use)
{
    auto it = find(operands.begin() + 1, operands.end(), use);
    if (it != operands.end())
        operands.erase(it);
}

std::vector<Operand *> Instruction::replaceAllUsesWith(Operand *replVal)
{
    auto def = getDef();
    if (def == nullptr)
        return std::vector<Operand *>();
    std::vector<Operand *> freeList;
    for (auto userInst : def->getUse())
    {
        for (auto &&i : userInst->getUse())
        {
            if (i->getEntry() == def->getEntry())
            {
                if (userInst->isPhi())
                {
                    auto &srcs = ((PhiInstruction *)userInst)->getSrcs();
                    for (auto &src : srcs)
                    {
                        if (src.second == i)
                            src.second = replVal;
                    }
                }
                freeList.push_back(i);
                i->removeUse(userInst);
                i = replVal;
                replVal->addUse(userInst);
            }
        }
    }
    return freeList;
}

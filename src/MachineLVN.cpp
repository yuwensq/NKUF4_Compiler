#include "MachineLVN.h"
#include "AsmBuilder.h"
#include <sstream>
// #define PRINTLOG

void MachineLVN::clearData()
{
    nextVN = 1;
    hashTable.clear();
    vn2Op.clear();
}

void MachineLVN::pass(MachineFunction *func)
{
    for (auto &blk : func->getBlocks())
    {
        clearData();
        auto instBegin = blk->getInsts().begin();
        auto instEnd = blk->getInsts().end();
        for (auto instIt = instBegin; instIt != instEnd; instIt++)
        {
            auto inst = *instIt;
            if (skip(inst))
            {
                if (inst->getDef().size() > 0)
                {
                    auto def = inst->getDef()[0];
                    auto hashKey = toStr(def);
                    vn2Op.erase(hashTable[hashKey]);
                    hashTable.erase(hashKey);
                }
                continue;
            }
            // 尝试替换为mov
            replaceWithMov(instIt);
            // 尝试常量折叠
            constFold(instIt);
            // 把新的表达式加进来
            addNewExpr(instIt);
        }
    }
}

void MachineLVN::pass()
{
#ifdef PRINTLOG
    Log("汇编局部值编号开始");
#endif
    clearData();
    for (auto &func : unit->getFuncs())
    {
        pass(func);
    }
#ifdef PRINTLOG
    Log("汇编局部值编号结束");
#endif
}

bool MachineLVN::skip(MachineInstruction *inst)
{
    // 这里先处理比较少的指令吧，汇编方面比较复杂
    // 涉及到传参用的寄存器，并不考虑
    for (auto use : inst->getUse())
    {
        if (use->isReg())
        {
            if (use->isFReg() && use->getReg() < 16)
                return true;
            if (!use->isFReg() && use->getReg() < 4)
                return true;
        }
    }
    if (inst->isBinary() || (inst->isLoad() && (inst->getUse()[0]->isImm() || inst->getUse()[0]->isLabel())) || inst->isMovClass())
        return false;
    return true;
}

std::string MachineLVN::toStr(MachineOperand *op)
{
    return op->toStr();
}

std::string MachineLVN::toStr(MachineInstruction *inst)
{
    for (auto use : inst->getUse())
    {
        auto hashKey = toStr(use);
        if (hashTable.find(hashKey) == hashTable.end())
            hashTable[hashKey] = (nextVN++);
    }
    std::string res = "";
    if (inst->isLoad() || inst->isMovClass())
        res = toStr(inst->getUse()[0]);
    else if (inst->isBinary())
    {
        std::string vn;
        std::stringstream ss;
        ss << hashTable[toStr(inst->getUse()[0])];
        ss >> vn;
        res = vn + " " + static_cast<BinaryMInstruction *>(inst)->opStr() + " ";
        vn = "";
        ss.clear();
        ss << hashTable[toStr(inst->getUse()[1])];
        ss >> vn;
        res += vn;
    }
    else
        assert(0);
    return res;
}

void MachineLVN::replaceWithMov(std::vector<MachineInstruction *>::iterator instIt)
{
    auto inst = *instIt;
    auto block = inst->getParent();
    // 这里只把binary或者ldr替换为mov指令
    if (inst->isMovClass())
        return;
    auto hashKey = toStr(inst);
    if (hashTable.find(hashKey) == hashTable.end())
        return;
    auto vn = hashTable[hashKey];
    if (vn2Op.find(vn) == vn2Op.end())
        return;
    auto op = vn2Op[vn];
    MachineInstruction *newInst = nullptr;
    if (op.isImm())
    {
        if (AsmBuilder::isLegalImm(op.getVal()))
            newInst = new MovMInstruction(block, MovMInstruction::MOV, new MachineOperand(*inst->getDef()[0]), new MachineOperand(op));
        else
            newInst = new LoadMInstruction(block, LoadMInstruction::LDR, new MachineOperand(*inst->getDef()[0]), new MachineOperand(op));
    }
    else
        newInst = new MovMInstruction(block, MovMInstruction::MOV, new MachineOperand(*inst->getDef()[0]), new MachineOperand(op));
    *instIt = newInst;
}

void MachineLVN::constFold(std::vector<MachineInstruction *>::iterator instIt)
{
}

void MachineLVN::addNewExpr(std::vector<MachineInstruction *>::iterator instIt)
{
    auto inst = *instIt;
    auto def = inst->getDef()[0];
    vn2Op.erase(hashTable[toStr(def)]);
    hashTable.erase(toStr(def));
    auto hashKey = toStr(inst);
    if (hashTable.find(hashKey) == hashTable.end())
        hashTable[hashKey] = (nextVN++);
    auto vn = hashTable[hashKey];
    hashTable[toStr(def)] = vn;
    vn2Op[vn] = *def;
}

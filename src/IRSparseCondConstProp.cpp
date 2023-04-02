#include "IRSparseCondConstProp.h"
#include "SymbolTable.h"
#include "debug.h"
#include <map>
#include <queue>
#include <unordered_set>
#include <set>
#include <vector>

enum State
{
    BOT = 0,
    CON,
    TOP
};

struct Lattice
{
    State state;
    ConstantSymbolEntry *constVal;
    Lattice()
    {
        state = BOT;
        constVal = nullptr;
    }
};

std::map<Operand *, Lattice> operandState;
std::queue<std::pair<BasicBlock *, BasicBlock *>> cfgWorkList;
std::queue<Instruction *> ssaWorkList;
std::set<std::pair<BasicBlock *, BasicBlock *>> edgeColor;

static Lattice getLatticeOfOp(Operand *ope)
{
    Assert(ope != nullptr, "ope不应为空");
    Lattice res;
    if (ope->getEntry()->isConstant())
    {
        res.state = CON;
        res.constVal = static_cast<ConstantSymbolEntry *>(ope->getEntry());
    }
    // 理论上这个都可以在map中找到，找不到就是哪里没考虑好
    else if (operandState.find(ope) != operandState.end())
    {
        res = operandState[ope];
    }
    return res;
}

static Lattice intersect(Lattice op1, Lattice op2)
{
    Lattice res;
    res.state = std::min(op1.state, op2.state);
    if (res.state == CON)
    {
        if (op1.state == CON && op2.state == CON)
        {
            // 一个phi指令的操作数应该类型一样吧
            Assert(op1.constVal->getType() == op2.constVal->getType(), "求交的格类型不同");
            if (op1.constVal->getValue() != op2.constVal->getValue())
            {
                res.state = BOT;
            }
            else
            {
                res.constVal = op1.constVal;
            }
        }
        else
        {
            // 这种情况应该是有一个为top
            res.constVal = (op1.state == TOP ? op2.constVal : op1.constVal);
        }
    }
    return res;
}

static Lattice constFold(Lattice op1, Lattice op2, Instruction *inst)
{
    Lattice res;
    res.state = TOP;
    if (op1.state == op2.state && op1.state == CON)
    {
        res.state = CON;
        bool isFloat = op1.constVal->getType()->isFloat();
        double v1 = op1.constVal->getValue();
        double v2 = op2.constVal->getValue();
        float fv1 = static_cast<float>(v1);
        float fv2 = static_cast<float>(v2);
        int iv1 = static_cast<int>(v1);
        int iv2 = static_cast<int>(v2);
        double resV = 0;
        if (dynamic_cast<BinaryInstruction *>(inst) != nullptr)
        {
            switch (inst->getOpCode())
            {
            case BinaryInstruction::ADD:
                resV = static_cast<double>(isFloat ? fv1 + fv2 : iv1 + iv2);
                break;
            case BinaryInstruction::SUB:
                resV = static_cast<double>(isFloat ? fv1 - fv2 : iv1 - iv2);
                break;
            case BinaryInstruction::MUL:
                resV = static_cast<double>(isFloat ? fv1 * fv2 : iv1 * iv2);
                break;
            case BinaryInstruction::DIV: // 这里用不用处理除零异常呢？
                resV = static_cast<double>(isFloat ? fv1 / fv2 : iv1 / iv2);
                break;
            case BinaryInstruction::MOD:
                Assert(!isFloat, "Mod运算的操作数为float");
                resV = static_cast<double>(iv1 % iv2);
                break;
            default:
                Assert(false, "未识别的操作");
                break;
            }
            res.constVal = new ConstantSymbolEntry(isFloat ? TypeSystem::floatType : TypeSystem::intType, resV);
        }
        else if (dynamic_cast<CmpInstruction *>(inst) != nullptr)
        {
            switch (inst->getOpCode())
            {
            case CmpInstruction::E:
                resV = static_cast<double>(isFloat ? fv1 == fv2 : iv1 == iv2);
                break;
            case CmpInstruction::NE:
                resV = static_cast<double>(isFloat ? fv1 != fv2 : iv1 != iv2);
                break;
            case CmpInstruction::G:
                resV = static_cast<double>(isFloat ? fv1 > fv2 : iv1 > iv2);
                break;
            case CmpInstruction::GE:
                resV = static_cast<double>(isFloat ? fv1 >= fv2 : iv1 >= iv2);
                break;
            case CmpInstruction::L:
                resV = static_cast<double>(isFloat ? fv1 < fv2 : iv1 < iv2);
                break;
            case CmpInstruction::LE:
                resV = static_cast<double>(isFloat ? fv1 <= fv2 : iv1 <= iv2);
                break;
            default:
                Assert(false, "未识别的操作");
                break;
            }
            // 这里把比较结果当成int来算吧，应该没啥影响
            res.constVal = new ConstantSymbolEntry(TypeSystem::intType, resV);
        }
        else
        {
            Assert(false, "不合法的指令类型");
        }
    }
    else
    {
        if (op1.state == BOT || op2.state == BOT)
            res.state = BOT;
    }
    return res;
}

static Lattice constFold(Lattice op1, Instruction *inst)
{
    Lattice res;
    res.state = op1.state;
    if (op1.state == CON)
    {
        ConstantSymbolEntry *constV = nullptr;
        double v1 = op1.constVal->getValue();
        if (dynamic_cast<ZextInstruction *>(inst) != nullptr)
        {
            // bool转int
            constV = new ConstantSymbolEntry(TypeSystem::intType, int(v1));
        }
        else if (dynamic_cast<XorInstruction *>(inst) != nullptr)
        {
            // not操作
            constV = new ConstantSymbolEntry(TypeSystem::intType, (int(v1) ? 0 : 1));
        }
        else if (dynamic_cast<F2IInstruction *>(inst) != nullptr)
        {
            // float转int操作
            constV = new ConstantSymbolEntry(TypeSystem::intType, int(float(v1)));
        }
        else if (dynamic_cast<I2FInstruction *>(inst) != nullptr)
        {
            // int转float操作
            constV = new ConstantSymbolEntry(TypeSystem::floatType, float(int(v1)));
        }
        else
        {
            Assert(false, "不合法的指令");
        }
        res.constVal = constV;
    }
    return res;
}

static bool isDifferent(Lattice &a, Lattice &b)
{
    // 状态不同显然不同
    if (a.state != b.state)
        return true;
    // 状态相同，如果都是常数，还要判断具体的值是否相等
    if (a.state != CON)
        return false;
    Assert(a.constVal->getType() == b.constVal->getType(), "作比较的两个格的const值类型不同");
    double valA = a.constVal->getValue();
    double valB = b.constVal->getValue();
    if (a.constVal->getType()->isInt())
        return static_cast<int>(valA) != static_cast<int>(valB);
    else if (a.constVal->getType()->isFloat())
        return static_cast<float>(valA) != static_cast<float>(valB);
    else
        Assert(false, "意料之外的const类型");
    assert(false);
}

static void addUseOfInst(Instruction *inst)
{
    Assert(inst->getDef(), "传入指令类型不对");
    auto useList = inst->getDef()->getUse();
    for (auto ins : useList)
    {
        ssaWorkList.push(ins);
    }
}

static void handleInst(Instruction *inst)
{
    auto bb = inst->getParent();
    Lattice oldLattic;
    if (inst->getDef())
        oldLattic = operandState[inst->getDef()];
    if (inst->isBinaryCal())
    {
        Lattice op1 = getLatticeOfOp(inst->getOperands()[1]);
        Lattice op2 = getLatticeOfOp(inst->getOperands()[2]);
        Lattice res = constFold(op1, op2, inst);
        operandState[inst->getDef()] = res;
        if (isDifferent(res, oldLattic))
            addUseOfInst(inst);
    }
    else if (inst->isUnaryCal())
    {
        Lattice op1 = getLatticeOfOp(inst->getOperands()[1]);
        Lattice res = constFold(op1, inst);
        operandState[inst->getDef()] = res;
        if (isDifferent(res, oldLattic))
            addUseOfInst(inst);
    }
    else if (inst->isPhi())
    {
        Lattice res = operandState[inst->getDef()];
        for (auto pa : static_cast<PhiInstruction *>(inst)->getSrcs())
        {
            if (edgeColor.count(std::make_pair(pa.first, bb)) != 0)
            {
                res = intersect(res, getLatticeOfOp(pa.second));
            }
        }
        if (isDifferent(res, oldLattic))
            addUseOfInst(inst);
    }
    else if (inst->isCond())
    {
        Lattice condState = getLatticeOfOp(inst->getOperands()[0]);
        // 条件是常数
        if (condState.state == CON)
        {
            // 为真
            if (condState.constVal->getValue())
            {
                cfgWorkList.push(std::make_pair(bb, static_cast<CondBrInstruction *>(inst)->getTrueBranch()));
            }
            // 为假
            else
            {
                cfgWorkList.push(std::make_pair(bb, static_cast<CondBrInstruction *>(inst)->getFalseBranch()));
            }
        }
        else
        {
            cfgWorkList.push(std::make_pair(bb, static_cast<CondBrInstruction *>(inst)->getTrueBranch()));
            cfgWorkList.push(std::make_pair(bb, static_cast<CondBrInstruction *>(inst)->getFalseBranch()));
        }
    }
    else if (inst->isUncond())
    {
        cfgWorkList.push(std::make_pair(bb, static_cast<UncondBrInstruction *>(inst)->getBranch()));
    }
    Assert(false, "剩下的指令不用处理了吧，数组相关的也算不出来常量吧");
}

static void replaceWithConst(std::vector<BasicBlock *> &blks)
{
    std::vector<Instruction *> removeList;
    for (auto bb : blks)
    {
        removeList.clear();
        auto inst = bb->begin();
        while (inst != bb->end())
        {
            if (inst->getDef() && operandState[inst->getDef()].state == CON)
            {
                // 结果为常数的inst
                removeList.push_back(inst);
            }
            if (dynamic_cast<CondBrInstruction *>(inst) != nullptr && operandState[inst->getOperands()[0]].state == CON)
            {
                // 这个指令可以替换为一条UnCondBr指令
                // 为真
                if (operandState[inst->getOperands()[0]].constVal->getValue())
                {
                }
                // 为假
                else
                {
                }
            }
            std::vector<Operand *> &ops = inst->getOperands();
            for (int i = 0; i < ops.size(); i++)
            {
                Lattice l = getLatticeOfOp(ops[i]);
                if (!ops[i]->getEntry()->isConstant() && l.state == CON)
                {
                    ops[i] = new Operand(l.constVal);
                }
            }
            inst = inst->getNext();
        }
        for (auto rInst : removeList)
        {
            bb->remove(rInst);
        }
    }
}

static void sccpInFunc(Function *func)
{
    // 清一下数据结构，不同函数之间应该也不会有啥共用的
    operandState.clear();
    edgeColor.clear();
    std::queue<std::pair<BasicBlock *, BasicBlock *>> empty1;
    std::swap(empty1, cfgWorkList);
    std::queue<Instruction *> empty2;
    std::swap(empty2, ssaWorkList);

    std::vector<BasicBlock *> blks(func->begin(), func->end());

    assert(blks.size() == func->getBlockList().size());

    for (auto bb : blks)
    {
        auto inst = bb->begin();
        while (inst != bb->end())
        {
            if (inst->getDef() != nullptr)
            {
                Lattice l;
                l.state = TOP;
                l.constVal = nullptr;
                // ssa每个操作数只会被定义一次，这里没问题
                operandState.insert(std::make_pair(inst->getDef(), l));
            }
            inst = inst->getNext();
        }
    }

    cfgWorkList.push(std::make_pair(nullptr, func->getEntry()));

    while (!cfgWorkList.empty() || !ssaWorkList.empty())
    {
        while (!cfgWorkList.empty())
        {
            auto pai = cfgWorkList.front();
            cfgWorkList.pop();
            if (edgeColor.count(pai) != 0)
                continue;
            edgeColor.insert(pai);
            auto inst = pai.second->begin();
            while (inst != pai.second->end())
            {
                handleInst(inst);
                inst = inst->getNext();
            }
        }
        while (!ssaWorkList.empty())
        {
            auto inst = ssaWorkList.front();
            ssaWorkList.pop();
            auto bb = inst->getParent();
            for (auto preBB = bb->pred_begin(); preBB != bb->pred_end(); preBB++)
            {
                if (edgeColor.count({*preBB, bb}) != 0)
                {
                    handleInst(inst);
                    break;
                }
            }
        }
    }

    replaceWithConst(blks);
}

void IRSparseCondConstProp::pass()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        sccpInFunc(*func);
    }
}
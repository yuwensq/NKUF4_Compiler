#include "IRSparseCondConstProp.h"
#include "SymbolTable.h"
#include "debug.h"
#include <map>
#include <queue>
#include <unordered_set>
#include <set>
#include <vector>

extern FILE *yyout;

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
            Assert(op1.constVal->getType() == op2.constVal->getType(), "求交的常量格类型不同");
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
        Assert(op1.constVal->getType() == op2.constVal->getType(), "指令的两个操作数类型不同");
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
                // 为啥这里用三目运算符出bug了
                if (isFloat)
                    resV = fv1 + fv2;
                else
                    resV = iv1 + iv2;
                break;
            case BinaryInstruction::SUB:
                if (isFloat)
                    resV = fv1 - fv2;
                else
                    resV = iv1 - iv2;
                break;
            case BinaryInstruction::MUL:
                if (isFloat)
                    resV = fv1 * fv2;
                else
                    resV = iv1 * iv2;
                break;
            case BinaryInstruction::DIV: // 这里用不用处理除零异常呢？
                Assert(fv2 != 0 && iv2 != 0, "除数不能为零");
                if (isFloat)
                    resV = fv1 / fv2;
                else
                    resV = iv1 / iv2;
                break;
            case BinaryInstruction::MOD:
                Assert(!isFloat, "Mod运算的操作数为float");
                resV = (iv1 % iv2);
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
                if (isFloat)
                    resV = (fv1 == fv2);
                else    
                    resV = (iv1 == iv2);
                break;
            case CmpInstruction::NE:
                if (isFloat)
                    resV = (fv1 != fv2);
                else    
                    resV = (iv1 != iv2);
                break;
            case CmpInstruction::G:
                if (isFloat)
                    resV = (fv1 > fv2);
                else    
                    resV = (iv1 > iv2);
                break;
            case CmpInstruction::GE:
                if (isFloat)
                    resV = (fv1 >= fv2);
                else    
                    resV = (iv1 >= iv2);
                break;
            case CmpInstruction::L:
                if (isFloat)
                    resV = (fv1 < fv2);
                else    
                    resV = (iv1 < iv2);
                break;
            case CmpInstruction::LE:
                if (isFloat)
                    resV = (fv1 <= fv2);
                else    
                    resV = (iv1 <= iv2);
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
            constV = new ConstantSymbolEntry(TypeSystem::intType, (int(v1) ? 1 : 0));
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
        // fprintf(yyout, "%d", res.state);
        if (isDifferent(res, oldLattic))
            addUseOfInst(inst);
    }
    else if (inst->isUnaryCal())
    {
        Lattice op1 = getLatticeOfOp(inst->getOperands()[1]);
        Lattice res = constFold(op1, inst);
        operandState[inst->getDef()] = res;
        // fprintf(yyout, "%d", res.state);
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
        operandState[inst->getDef()] = res;
        // fprintf(yyout, "%d", res.state);
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
    else if (inst->isLoad() || inst->isCall())
    {
        Lattice res;
        if (inst->getDef())
            operandState[inst->getDef()] = res;
    }
}

static void replaceWithConst(Function *func)
{
    std::vector<Instruction *> removeList;
    for (auto it = func->begin(); it != func->end(); it++)
    {
        auto bb = *it;
        removeList.clear();
        auto inst = bb->begin();
        while (inst != bb->end())
        {
            if (inst->getDef() && operandState[inst->getDef()].state == CON)
            {
                // 结果为常数的inst
                removeList.push_back(inst);
                inst = inst->getNext();
                continue;
            }
            if (dynamic_cast<CondBrInstruction *>(inst) != nullptr && operandState[inst->getOperands()[0]].state == CON)
            {
                // 这个指令可以替换为一条UnCondBr指令
                // 为真
                removeList.push_back(inst);
                UncondBrInstruction *newInst = nullptr;
                BasicBlock *trueBranch = static_cast<CondBrInstruction *>(inst)->getTrueBranch();
                BasicBlock *falseBranch = static_cast<CondBrInstruction *>(inst)->getFalseBranch();
                if (operandState[inst->getOperands()[0]].constVal->getValue())
                {
                    newInst = new UncondBrInstruction(trueBranch, nullptr);
                    if (trueBranch != falseBranch)
                    {
                        bb->removeSucc(falseBranch);
                        falseBranch->removePred(bb);
                    }
                }
                // 为假
                else
                {
                    newInst = new UncondBrInstruction(falseBranch, nullptr);
                    if (trueBranch != falseBranch)
                    {
                        bb->removeSucc(trueBranch);
                        trueBranch->removePred(bb);
                    }
                }
                bb->insertBefore(newInst, inst);
            }
            std::vector<Operand *> &ops = inst->getOperands();
            for (int i = 0; i < ops.size(); i++)
            {
                if (ops[i] == nullptr)
                    continue;
                Lattice l = getLatticeOfOp(ops[i]);
                // 源操作数不是常量，但是分析后为常量
                if (!ops[i]->getEntry()->isConstant() && l.state == CON)
                {
                    inst->replaceUse(ops[i], new Operand(l.constVal));
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
            if (edgeColor.find(pai) != edgeColor.end())
                continue;
            edgeColor.insert(pai);
            auto inst = pai.second->begin();
            while (inst != pai.second->end())
            {
                handleInst(inst);
                // inst->output();
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
                if (edgeColor.find({*preBB, bb}) != edgeColor.end())
                {
                    handleInst(inst);
                    // inst->output();
                    break;
                }
            }
        }
    }

    replaceWithConst(func);
}

void IRSparseCondConstProp::pass()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        sccpInFunc(*func);
    }
}
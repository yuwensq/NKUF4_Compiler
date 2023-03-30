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
        // 这样应该没问题吧
        assert(op1.constVal->getType() == op2.constVal->getType());
        if (op1.constVal->getValue() != op2.constVal->getValue())
        {
            res.state = BOT;
        }
        else
        {
            res.constVal = op1.constVal;
        }
    }
    return res;
}

static Lattice constFold(Lattice op1, Lattice op2, Instruction *inst)
{
    Lattice res;
    if (op1.state == op2.state && op1.state == CON)
    {
        res.state = CON;
        bool isFloat = op1.constVal->getType()->isFloat();
        double v1 = op1.constVal->getValue();
        double v2 = op2.constVal->getValue();
        double resV = 0;
        if (dynamic_cast<BinaryInstruction *>(inst) != nullptr)
        {
            switch (inst->getOpCode())
            {
            case BinaryInstruction::ADD:
                resV = (isFloat ? float(v1) + float(v2) : int(v1) + int(v2));
                break;
            case BinaryInstruction::SUB:
                resV = (isFloat ? float(v1) - float(v2) : int(v1) - int(v2));
                break;
            case BinaryInstruction::MUL:
                resV = (isFloat ? float(v1) * float(v2) : int(v1) * int(v2));
                break;
            case BinaryInstruction::DIV: // 这里用不用处理除零异常呢？
                resV = (isFloat ? float(v1) / float(v2) : int(v1) / int(v2));
                break;
            case BinaryInstruction::MOD:
                Assert(!isFloat, "Mod运算的操作数为float");
                resV = int(v1) % int(v2);
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
                resV = (isFloat ? float(v1) == float(v2) : int(v1) == int(v2));
                break;
            case CmpInstruction::NE:
                resV = (isFloat ? float(v1) != float(v2) : int(v1) != int(v2));
                break;
            case CmpInstruction::G:
                resV = (isFloat ? float(v1) > float(v2) : int(v1) > int(v2));
                break;
            case CmpInstruction::GE:
                resV = (isFloat ? float(v1) >= float(v2) : int(v1) >= int(v2));
                break;
            case CmpInstruction::L:
                resV = (isFloat ? float(v1) < float(v2) : int(v1) < int(v2));
                break;
            case CmpInstruction::LE:
                resV = (isFloat ? float(v1) <= float(v2) : int(v1) <= int(v2));
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
        res.state = std::min(op1.state, op2.state);
    }
    return res;
}

static Lattice constFold(Lattice op1, Instruction *inst)
{
}

static void addUseOfInst(Instruction *inst, State newState, State oldState)
{
}

static void handleInst(Instruction *inst)
{
    State oldState = (inst->getDef() != nullptr ? operandState[inst->getDef()].state : TOP);
    if (inst->isBinaryCal())
    {
        Lattice op1 = getLatticeOfOp(inst->getOperands()[1]);
        Lattice op2 = getLatticeOfOp(inst->getOperands()[2]);
        Lattice res = constFold(op1, op2, inst);
        operandState[inst->getDef()] = res;
        addUseOfInst(inst, res.state, oldState);
    }
    else if (inst->isUnaryCal())
    {
        Lattice op1 = getLatticeOfOp(inst->getOperands()[1]);
        Lattice res = constFold(op1, inst);
        operandState[inst->getDef()] = res;
        addUseOfInst(inst, res.state, oldState);
    }
    else if (inst->isPhi())
    {
    }
    else if (inst->isCond())
    {
    }
    else if (inst->isUncond())
    {
    }
}

static void replaceWithConst()
{
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

    replaceWithConst();
}

void IRSparseCondConstProp::pass()
{
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        sccpInFunc(*func);
    }
}
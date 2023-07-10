#include "GraphColor.h"
#include "debug.h"
#include <queue>
extern FILE *yyout;

// #define DEBUGGC1
// #define DEBUGGC2

void GraphColor::clearData()
{
    nodes.clear();
    graph.clear();
    var2Node.clear();
    spillNodes.clear();
    std::stack<int>().swap(colorSeq);
}

void GraphColor::debug1(std::map<MachineBlock *, std::set<MachineOperand *>> &gen, std::map<MachineBlock *, std::set<MachineOperand *>> &kill, std::map<MachineBlock *, std::set<MachineOperand *>> &in, std::map<MachineBlock *, std::set<MachineOperand *>> &out)
{
#ifdef DEBUGGC1
    fprintf(yyout, "genkill");
    for (auto &block : func->getBlocks())
    {
        fprintf(yyout, "\n%d\n\t", block->getNo());
        for (auto &ge : gen[block])
        {
            ge->output();
            fprintf(yyout, " ");
        }
        fprintf(yyout, "\n\t");
        for (auto &ki : kill[block])
        {
            ki->output();
            fprintf(yyout, " ");
        }
    }
    fprintf(yyout, "\ninout");
    for (auto &block : func->getBlocks())
    {
        fprintf(yyout, "\n%d\n\t", block->getNo());
        for (auto &i : in[block])
        {
            i->output();
            fprintf(yyout, " ");
        }
        fprintf(yyout, "\n\t");
        for (auto &ou : out[block])
        {
            ou->output();
            fprintf(yyout, " ");
        }
    }
    for (auto &block : func->getBlocks())
        Log("%d %d %d", block->getNo(), in[block].size(), out[block].size());
#endif
}

void GraphColor::debug2(std::map<MachineBlock *, std::set<int>> &gen, std::map<MachineBlock *, std::set<int>> &kill, std::map<MachineBlock *, std::set<int>> &in, std::map<MachineBlock *, std::set<int>> &out)
{
#ifdef DEBUGGC2
    fprintf(yyout, "\ngenkill");
    for (auto &block : func->getBlocks())
    {
        fprintf(yyout, "\n%d\n\t", block->getNo());
        for (auto g : gen[block])
        {
            fprintf(yyout, "%d ", g);
        }
        fprintf(yyout, "\n\t");
        for (auto k : kill[block])
        {
            fprintf(yyout, "%d ", k);
        }
    }
    fprintf(yyout, "\ninout");
    for (auto &block : func->getBlocks())
    {
        fprintf(yyout, "\n%d\n\t", block->getNo());
        for (auto i : in[block])
        {
            fprintf(yyout, "%d ", i);
        }
        fprintf(yyout, "\n\t");
        for (auto o : out[block])
        {
            fprintf(yyout, "%d ", o);
        }
    }
#endif
}

void GraphColor::aggregate(std::set<MachineOperand *> &a, std::set<MachineOperand *> &b, std::set<MachineOperand *> &out)
{
    out.clear();
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), inserter(out, out.begin()));
}

void GraphColor::aggregate(std::set<int> &a, std::set<int> &b, std::set<int> &out)
{
    out.clear();
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), inserter(out, out.begin()));
}

void GraphColor::calDRGenKill(std::map<MachineBlock *, std::set<MachineOperand *>> &gen, std::map<MachineBlock *, std::set<MachineOperand *>> &kill)
{
    for (auto &block : func->getBlocks())
    {
        for (auto &inst : block->getInsts())
        {
            if (inst->getDef().size() > 0)
            {
                assert(inst->getDef().size() == 1);
                // 学学新用法
                auto def = inst->getDef()[0];
                if (!def->isVReg())
                    continue;
                nodes.emplace_back(def->isFReg(), def);
                var2Node[def] = nodes.size() - 1;
                if (spilledRegs.count(def) > 0)
                    nodes[var2Node[def]].hasSpilled = true;
                auto copyGen = gen[block];
                for (auto op : copyGen)
                    if ((*op) == (*def))
                        gen[block].erase(op);
                gen[block].insert(def);
                kill[block].insert(def);
            }
        }
    }
}

void GraphColor::calDRInOut(std::map<MachineBlock *, std::set<MachineOperand *>> &gen, std::map<MachineBlock *, std::set<MachineOperand *>> &kill, std::map<MachineBlock *, std::set<MachineOperand *>> &in, std::map<MachineBlock *, std::set<MachineOperand *>> &out)
{
    auto entry = func->getEntry();
    out[entry] = gen[entry];
    std::queue<MachineBlock *> workList;
    for (auto block : func->getBlocks())
        if (block != entry)
            workList.push(block);
    while (!workList.empty())
    {
        auto block = workList.front();
        workList.pop();
        std::set<MachineOperand *> In[2];
        In[0] = out[block->getPreds()[0]];
        auto it = block->getPreds().begin() + 1;
        auto overPos = block->getPreds().end();
        int turn = 0;
        for (; it != overPos; it++)
        {
            aggregate(out[*it], In[turn], In[turn ^ 1]);
            turn ^= 1;
        }
        in[block] = In[turn];
        // 计算out
        std::set<MachineOperand *> Out;
        for (auto indef : in[block])
            for (auto def : kill[block])
                if (*indef == *def)
                    In[turn].erase(indef);
        std::set_union(gen[block].begin(), gen[block].end(), In[turn].begin(), In[turn].end(), inserter(Out, Out.begin()));
        if (out[block] != Out)
        {
            out[block] = Out;
            for (auto &succ : block->getSuccs())
                workList.push(succ);
        }
    }
}

int GraphColor::mergeTwoNodes(int no1, int no2)
{
    int dst = std::min(no1, no2);
    int src = std::max(no1, no2);
    bool spilled = (nodes[dst].hasSpilled || nodes[src].hasSpilled);
    assert(nodes[src].fpu == nodes[dst].fpu);
    if (dst == src)
        return dst;
    nodes[dst].hasSpilled = spilled;
    nodes[src].hasSpilled = spilled;
    nodes[dst].defs.insert(nodes[src].defs.begin(), nodes[src].defs.end());
    nodes[dst].uses.insert(nodes[src].uses.begin(), nodes[src].uses.end());
    for (auto &def : nodes[dst].defs)
        var2Node[def] = dst;
    for (auto &use : nodes[dst].uses)
        var2Node[use] = dst;
    return dst;
}

void GraphColor::genNodes()
{
    std::map<MachineBlock *, std::set<MachineOperand *>> gen;
    std::map<MachineBlock *, std::set<MachineOperand *>> kill;
    std::map<MachineBlock *, std::set<MachineOperand *>> in;
    std::map<MachineBlock *, std::set<MachineOperand *>> out;
    calDRGenKill(gen, kill);
    calDRInOut(gen, kill, in, out);
    debug1(gen, kill, in, out);
    // 这个主要就是一个运算数可能有多个def都可以到达他
    std::map<MachineOperand, std::set<int>> op2Def;
    for (auto &block : func->getBlocks())
    {
        op2Def.clear();
        for (auto &def : in[block])
            op2Def[*def].insert(var2Node[def]);
        for (auto &inst : block->getInsts())
        {
            for (auto &use : inst->getUse())
            {
                if (!use->isVReg())
                    continue;
                // 这里如果小于等于零，表示当前use的定值不存在，不太可能
                assert(op2Def[*use].size() > 0);
                auto it = op2Def[*use].begin();
                auto overPos = op2Def[*use].end();
                int no1 = *it;
                it++;
                for (; it != overPos; it++)
                {
                    int no2 = *it;
                    no1 = mergeTwoNodes(no1, no2);
                }
                op2Def[*use].clear();
                op2Def[*use].insert(no1);
                nodes[no1].uses.insert(use);
                var2Node[use] = no1;
            }
            if (inst->getDef().size() > 0)
            {
                auto def = inst->getDef()[0];
                if (def->isVReg())
                {
                    op2Def.erase(*def);
                    op2Def[*def].insert(var2Node[def]);
                }
            }
        }
    }
    // for (auto &block : func->getBlocks())
    // {
    //     fprintf(yyout, ".L%d\n", block->getNo());
    //     for (auto &inst : block->getInsts())
    //     {
    //         for (auto &def : inst->getDef())
    //         {
    //             if (def->isVReg())
    //                 //     def->setRegNo(var2Node[def]);
    //                 fprintf(yyout, "%d ", var2Node[def]);
    //         }
    //         for (auto &use : inst->getUse())
    //         {
    //             if (use->isVReg())
    //                 // use->setRegNo(var2Node[use]);
    //                 fprintf(yyout, "%d ", var2Node[use]);
    //         }
    //         inst->output();
    //     }
    // }
}

void GraphColor::calLVGenKill(std::map<MachineBlock *, std::set<int>> &gen, std::map<MachineBlock *, std::set<int>> &kill)
{
    for (auto &block : func->getBlocks())
    {
        for (auto instIt = block->rbegin(); instIt != block->rend(); instIt++)
        {
            auto inst = *instIt;
            if (inst->getDef().size() > 0)
            {
                assert(inst->getDef().size() == 1);
                auto def = inst->getDef()[0];
                if (def->isVReg())
                {
                    gen[block].erase(var2Node[def]);
                    kill[block].insert(var2Node[def]);
                }
            }
            for (auto &use : inst->getUse())
            {
                if (!use->isVReg())
                    continue;
                gen[block].insert(var2Node[use]);
            }
        }
    }
}

void GraphColor::calLVInOut(std::map<MachineBlock *, std::set<int>> &gen, std::map<MachineBlock *, std::set<int>> &kill, std::map<MachineBlock *, std::set<int>> &in, std::map<MachineBlock *, std::set<int>> &out)
{
    std::queue<MachineBlock *> workList;
    for (auto &block : func->getBlocks())
    {
        if (block->getSuccs().size() == 0)
        {
            in[block] = gen[block];
            continue;
        }
        workList.push(block);
    }
    while (!workList.empty())
    {
        auto block = workList.front();
        workList.pop();
        std::set<int> Out[2];
        Out[0] = in[block->getSuccs()[0]];
        auto it = block->getSuccs().begin() + 1;
        auto overPos = block->getSuccs().end();
        int turn = 0;
        for (; it != overPos; it++)
        {
            aggregate(in[*it], Out[turn], Out[turn ^ 1]);
            turn ^= 1;
        }
        out[block] = Out[turn];
        // 计算out
        std::set<int> In;
        std::set<int> midDif;
        std::set_difference(out[block].begin(), out[block].end(), kill[block].begin(), kill[block].end(), inserter(midDif, midDif.begin()));
        std::set_union(gen[block].begin(), gen[block].end(), midDif.begin(), midDif.end(), inserter(In, In.begin()));
        if (in[block] != In)
        {
            in[block] = In;
            for (auto &pred : block->getPreds())
                workList.push(pred);
        }
    }
}

void GraphColor::genInterfereGraph()
{
    std::map<MachineBlock *, std::set<int>> gen;
    std::map<MachineBlock *, std::set<int>> kill;
    std::map<MachineBlock *, std::set<int>> in;
    std::map<MachineBlock *, std::set<int>> out;
    calLVGenKill(gen, kill);
    calLVInOut(gen, kill, in, out);
    debug2(gen, kill, in, out);

    auto connectEdge = [&](MachineOperand *op, MachineBlock *block)
    {
        auto use1 = var2Node[op];
        graph[use1];
        for (auto &use2 : out[block])
        {
            if (nodes[use1].fpu == nodes[use2].fpu)
            {
                graph[use1].insert(use2);
                graph[use2].insert(use1);
            }
        }
    };

    for (auto &block : func->getBlocks())
    {
        for (auto instIt = block->rbegin(); instIt != block->rend(); instIt++)
        {
            auto inst = *instIt;
            if (inst->getDef().size() > 0)
            {
                assert(inst->getDef().size() == 1);
                auto def = inst->getDef()[0];
                if (def->isVReg())
                {
                    connectEdge(def, block);
                    out[block].erase(var2Node[def]);
                    graph[var2Node[def]];
                }
            }
            for (auto &use : inst->getUse())
            {
                if (!use->isVReg())
                    continue;
                connectEdge(use, block);
                // auto use1 = var2Node[use];
                // graph[use1];
                // for (auto &use2 : out[block])
                // {
                //     // 类型得一样
                //     if (nodes[use1].fpu == nodes[use2].fpu)
                //     {
                //         graph[use1].insert(use2);
                //         graph[use2].insert(use1);
                //     }
                // }
                out[block].insert(var2Node[use]);
            }
        }
    }
    // 不能有自环
    for (auto &node : graph)
        node.second.erase(node.first);
}

typedef std::pair<int, int> ele;

struct cmp
{
    bool operator()(const ele &a, const ele &b)
    {
        return a.second > b.second;
    }
};

void GraphColor::genColorSeq()
{
    auto tmpGraph = graph;

    auto eraseNode = [&](std::map<int, std::unordered_set<int>>::iterator it)
    {
        int nodeNo = (*it).first;
        for (auto to : tmpGraph[nodeNo])
            tmpGraph[to].erase(nodeNo);
        return tmpGraph.erase(it);
    };

    while (!tmpGraph.empty())
    {
        bool blocked = true; // 表示是否还可能有节点可以染色
        auto maxEdgesIt = tmpGraph.begin();
        int maxEdges = 0;
        for (auto it = tmpGraph.begin(); it != tmpGraph.end();)
        {
            int nodeNo = (*it).first;
            int edges = (*it).second.size();
            // Log("%d", nodeNo);
            int maxColors = nodes[nodeNo].fpu ? sRegNum : rRegNum;
            if (edges > maxEdges && !nodes[nodeNo].hasSpilled)
            {
                maxEdges = edges;
                maxEdgesIt = it;
            }
            if (edges < maxColors)
            {
                blocked = false;
                colorSeq.push(nodeNo);
                it = eraseNode(it);
            }
            else
                it++;
        }
        if (!blocked)
            continue;
        colorSeq.push((*maxEdgesIt).first);
        eraseNode(maxEdgesIt);
    }

    assert(colorSeq.size() == graph.size());
}

int GraphColor::findMinValidColor(int nodeNo)
{
    std::set<int> usedColor;
    for (auto to : graph[nodeNo])
        if (nodes[to].color != -1)
            usedColor.insert(nodes[to].color);
    int validColor = (nodes[nodeNo].fpu ? sbase : rbase);
    int maxValidColor = (nodes[nodeNo].fpu ? sRegNum : rRegNum) + validColor;
    for (; validColor < maxValidColor && usedColor.find(validColor) != usedColor.end(); validColor++)
        ;
    if (validColor >= maxValidColor)
        validColor = -1;
    return validColor;
}

bool GraphColor::tryColor()
{
    bool success = true;
    // Log("%d", colorSeq.size());
    while (!colorSeq.empty())
    {
        int nodeNo = colorSeq.top();
        colorSeq.pop();
        int validColor = findMinValidColor(nodeNo);
        if (validColor != -1)
        {
            nodes[nodeNo].color = validColor;
        }
        else
        {
            success = false;
            nodes[nodeNo].spill = true;
            spillNodes.push_back(nodeNo);
        }
    }
    return success;
}

bool GraphColor::tryColoring()
{
    genColorSeq();
    return tryColor();
}

bool GraphColor::graphColorRegisterAlloc()
{
    clearData();
    genNodes();
    // for (int i = 0; i < nodes.size(); i++)
    // {
    //     fprintf(yyout, "\n%d\n", i);
    //     for (auto def : nodes[i].defs)
    //     {
    //         def->output();
    //         fprintf(yyout, " ");
    //     }
    //     fprintf(yyout, "\n");
    //     for (auto use : nodes[i].uses)
    //     {
    //         use->output();
    //         fprintf(yyout, " ");
    //     }
    // }
    genInterfereGraph();
    // for (auto it : graph)
    // {
    //     fprintf(yyout, "\n%d\n", it.first);
    //     for (auto to : it.second)
    //     {
    //         fprintf(yyout, "%d ", to);
    //     }
    //     fprintf(yyout, "\n");
    // }
    // coalescing();
    return tryColoring();
    // return true;
}

void GraphColor::modifyCode()
{
    int i = -1;
    for (auto &node : nodes)
    {
        i++;
        if (node.color == -1)
            continue;
        func->addSavedRegs(node.color);
        for (auto def : node.defs)
        {
            // assert(def->isVReg());
            def->setReg(node.color);
        }
        for (auto use : node.uses)
        {
            // assert(use->isVReg());
            use->setReg(node.color);
        }
    }
}

void GraphColor::allocateRegisters()
{
    spilledRegs.clear();
    int counter = 0;
    for (auto &f : unit->getFuncs())
    {
        func = f;
        bool success = false;
        while (!success)
        {
            success = graphColorRegisterAlloc();
            counter++;
            Log("counter %d", counter);
            // f->output();
            if (!success)
                genSpillCode();
            else
                modifyCode();
        }
    }
}

void GraphColor::genSpillCode()
{
    for (auto nodeNo : spillNodes)
    {
        auto node = nodes[nodeNo];
        if (!node.spill)
            continue;
        // 在栈中分配内存
        node.disp = func->AllocSpace(4) * -1;
        // 在use前插入ldr
        for (auto use : node.uses)
        {
            spilledRegs.insert(use);
            auto cur_bb = use->getParent()->getParent();
            MachineInstruction *cur_inst = nullptr;
            auto newUse = new MachineOperand(*use);
            spilledRegs.insert(newUse);
            if (node.fpu)
            {
                if (node.disp >= -1020)
                {
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::VLDR,
                                                    newUse,
                                                    new MachineOperand(MachineOperand::REG, 11),
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                }
                else
                {
                    auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    internal_reg,
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                    cur_inst = new BinaryMInstruction(cur_bb,
                                                      BinaryMInstruction::ADD,
                                                      new MachineOperand(*internal_reg),
                                                      new MachineOperand(*internal_reg),
                                                      new MachineOperand(MachineOperand::REG, 11));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::VLDR,
                                                    newUse,
                                                    new MachineOperand(*internal_reg));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                }
            }
            else
            {
                // https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/ldr--immediate-offset-?lang=en
                if (node.disp >= -4095)
                {
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    newUse,
                                                    new MachineOperand(MachineOperand::REG, 11),
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                }
                else
                {
                    auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    internal_reg,
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    newUse,
                                                    new MachineOperand(MachineOperand::REG, 11),
                                                    new MachineOperand(*internal_reg));
                    cur_bb->insertBefore(cur_inst, use->getParent());
                }
            }
        }

        // 在def后插入str
        for (auto def : node.defs)
        {
            spilledRegs.insert(def);
            auto cur_bb = def->getParent()->getParent();
            MachineInstruction *cur_inst = nullptr;
            auto newDef = new MachineOperand(*def);
            spilledRegs.insert(newDef);
            if (node.fpu)
            {
                if (node.disp >= -1020)
                {
                    cur_inst = new StoreMInstruction(cur_bb,
                                                     StoreMInstruction::VSTR,
                                                     newDef,
                                                     new MachineOperand(MachineOperand::REG, 11),
                                                     new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertAfter(cur_inst, def->getParent());
                }
                else
                {
                    auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    internal_reg,
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertAfter(cur_inst, def->getParent());
                    auto cur_inst1 = new BinaryMInstruction(cur_bb,
                                                            BinaryMInstruction::ADD,
                                                            new MachineOperand(*internal_reg),
                                                            new MachineOperand(*internal_reg),
                                                            new MachineOperand(MachineOperand::REG, 11));
                    cur_bb->insertAfter(cur_inst1, cur_inst);
                    auto cur_inst2 = new StoreMInstruction(cur_bb,
                                                           StoreMInstruction::VSTR,
                                                           newDef,
                                                           new MachineOperand(*internal_reg));

                    cur_bb->insertAfter(cur_inst2, cur_inst1);
                }
            }
            else
            {
                // https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/ldr--immediate-offset-?lang=en
                if (node.disp >= -4095)
                {
                    cur_inst = new StoreMInstruction(cur_bb,
                                                     StoreMInstruction::STR,
                                                     newDef,
                                                     new MachineOperand(MachineOperand::REG, 11),
                                                     new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertAfter(cur_inst, def->getParent());
                }
                else
                {
                    auto internal_reg = new MachineOperand(MachineOperand::VREG, SymbolTable::getLabel());
                    cur_inst = new LoadMInstruction(cur_bb,
                                                    LoadMInstruction::LDR,
                                                    internal_reg,
                                                    new MachineOperand(MachineOperand::IMM, node.disp));
                    cur_bb->insertAfter(cur_inst, def->getParent());
                    auto cur_inst1 = new StoreMInstruction(cur_bb,
                                                           StoreMInstruction::STR,
                                                           newDef,
                                                           new MachineOperand(MachineOperand::REG, 11),
                                                           new MachineOperand(*internal_reg));
                    cur_bb->insertAfter(cur_inst1, cur_inst);
                }
            }
        }
    }
}
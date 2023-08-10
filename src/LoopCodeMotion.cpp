#include "LoopCodeMotion.h"
#include <algorithm>
#include "LoopUnroll.h"
#include "PureFunctionAnalyser.h"

PureFunctionAnalyser *pureFunc1 = nullptr; // 检测纯函数

void LoopCodeMotion::clearData()
{
    DomBBSet.clear();
    LoopConst.clear();
    loopStoreOperands.clear();
}

// 代码外提
void LoopCodeMotion::pass()
{
    clearData();
    // 遍历每一个函数做操作
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        // 计算当前函数每一个基本块的必经节点，存入DomBBSet
        calculateFinalDomBBSet(*func);
        // printDomBB(*func);

        // 计算当前函数的回边集合
        std::vector<std::pair<BasicBlock *, BasicBlock *>> BackEdges = getBackEdges(*func);
        // printBackEdges(BackEdges);

        // 获取回边组，要求每个组中的回边的到达节点是同一个
        std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> edgeGroups = mergeEdge(BackEdges);
        // printEdgeGroups(edgeGroups);
        
        // 查找当前函数的循环体的集合
        std::vector<std::vector<BasicBlock *>> LoopList = calculateLoopList(*func, edgeGroups);
        // printLoop(LoopList);

        // 代码外提
        CodePullUp(*func, LoopList, BackEdges);
        dealwithNoPreBB(*func);
    }
}

// 循环展开优化，放这边是因为需要DomSet和LoopList的信息
bool LoopCodeMotion::pass1()
{
    clearData();
    bool flag = false;
    // 遍历每一个函数做操作
    for (auto func = unit->begin(); func != unit->end(); func++)
    {
        // 计算当前函数每一个基本块的必经节点，存入DomBBSet
        calculateFinalDomBBSet(*func);
        // printDomBB(*func);

        // 计算当前函数的回边集合
        std::vector<std::pair<BasicBlock *, BasicBlock *>> BackEdges = getBackEdges(*func);
        // printBackEdges(BackEdges);

        // 获取回边组，要求每个组中的回边的到达节点是同一个
        std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> edgeGroups = mergeEdge(BackEdges);
        // printEdgeGroups(edgeGroups);

        // 查找当前函数的循环体的集合
        std::vector<std::vector<BasicBlock *>> LoopList = calculateLoopList(*func, edgeGroups);
        // printLoop(LoopList);

        // 代码外提,但是一旦外提，dom和loop的信息就可能变化
        //  CodePullUp(*func,LoopList,BackEdges);
        //  dealwithNoPreBB(*func);

        // 循环展开
        LoopUnroll Ln(DomBBSet);
        Ln.calculateCandidateLoop(LoopList); // 计算候选的，待处理的循环集合
        Ln.Unroll();
        if (Ln.successUnroll)
        {
            flag = true;
        }
    }
    //cout<<flag<<endl;
    return flag;
}

// 计算当前函数每一个基本块的必经节点，存入DomBBSet
void LoopCodeMotion::calculateFinalDomBBSet(Function *func)
{
    // 初始化当前函数的domBBMap，入口基本块设它的必经节点为本身，其他基本块设他的必经节点为全部基本块的集合
    initializeDomBBSet(func);
    // 获取DomBBSet[func],存入lastSet，用于比较
    std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> lastSet(getDomBBSet(func));

    while (true)
    {
        // 获取DomBBSet[func],存入DomSet，我们每一轮迭代在DomSet上修改
        std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> DomSet(getDomBBSet(func));

        // 遍历当前函数每一个基本块，获取他们的必经节点
        for (auto block : func->getBlockList())
        {
            std::vector<BasicBlock *> finalDomSet;
            bool firstFlag = true;
            // 遍历当前基本块的每一个前继
            // 如果只有一个前继基本块，那么前继基本块的必经节点一定是当前基本块的必经节点
            // 如果有多个前继基本块，那么那些前继基本块共有的必经节点一定是当前基本块的必经节点
            for (auto fartherBlock = block->pred_begin(); fartherBlock != block->pred_end(); fartherBlock++)
            {
                if (firstFlag)
                {
                    finalDomSet = DomSet[*fartherBlock];
                    firstFlag = false;
                    continue;
                }
                finalDomSet = getIntersectBBList(DomSet[*fartherBlock], finalDomSet);
            }
            // 当前节点的支配节点集合必定包括它自身
            if (!count(finalDomSet.begin(), finalDomSet.end(), block))
            {
                finalDomSet.push_back(block);
            }
            DomSet[block] = finalDomSet;
        }

        // 与迭代前的那一版DomBBSet做比较，如果相同，就代表迭代完成，否则继续迭代
        DomBBSet[func] = DomSet;
        if (!ifDomBBSetChange(lastSet, func))
        {
            break;
        }
        else
        {
            lastSet = getDomBBSet(func);
        }
    }
    return;
}

// 对当前函数初始化DomBBSet
void LoopCodeMotion ::initializeDomBBSet(Function *func)
{
    // 如果当前的函数还没有计算domBBMap的话，初始化一个给它
    if (DomBBSet.find(func) == DomBBSet.end())
    {
        std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> domBBMap;
        DomBBSet[func] = domBBMap;
    }
    // 遍历当前函数的每一个基本块
    for (auto block : func->getBlockList())
    {
        // 如果当前函数的当前基本块还没有计算必经节点
        if (DomBBSet[func].find(block) == DomBBSet[func].end())
        {
            // 如果当前基本块是函数的入口点
            if (block == func->getEntry())
            {
                std::vector<BasicBlock *> dom_list;
                dom_list.push_back(func->getEntry());
                // 入口基本块的必经节点只有它自己（只支配它自己）
                DomBBSet[func][block] = dom_list;
            }
            else
            {
                // 如果不是入口基本块，就先设必经节点是当前函数全部基本块的集合
                std::vector<BasicBlock *> dom_list(func->getBlockList());
                DomBBSet[func][block] = dom_list;
            }
        }
    }
    return;
}

// 找出两个序列中共有的那些个节点
std::vector<BasicBlock *> LoopCodeMotion::getIntersectBBList(std::vector<BasicBlock *> &List1, std::vector<BasicBlock *> &List2)
{
    std::vector<BasicBlock *> finalList;
    for (auto block : List1)
    {
        if (count(List2.begin(), List2.end(), block))
        {
            finalList.push_back(block);
        }
    }
    return finalList;
}

// 比较DomBBSet是否发生了改变
bool LoopCodeMotion ::ifDomBBSetChange(std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> &lastSet, Function *func)
{
    std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> DomBB_map = getDomBBSet(func);
    for (auto block : func->getBlockList())
    {
        if (lastSet[block] != DomBB_map[block])
        {
            return true;
        }
    }
    return false;
}

void LoopCodeMotion::printDomBB(Function *func)
{
    std::cout << "domBB:" << std::endl;
    for (auto block : (func)->getBlockList())
    {
        std::cout << block->getNo() << ":";
        for (auto dom : DomBBSet[func][block])
        {
            std::cout << dom->getNo() << " ";
        }
        std::cout << std::endl;
    }
}

// 获取回边（如果存在从节点i到j的有向边，并且j是i的一个必经节点，那么这条边就是回边）
std::vector<std::pair<BasicBlock *, BasicBlock *>> LoopCodeMotion::getBackEdges(Function *func)
{
    // 先获取DomBBSet
    std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> DomSet = getDomBBSet(func);
    std::vector<std::pair<BasicBlock *, BasicBlock *>> BackEdges;
    // search for backedges
    // std::cout<<"backedges:"<<std::endl;
    // 遍历当前基本块的每一个必经节点
    for (auto block : func->getBlockList())
    {
        for (auto domBB : DomSet[block])
        {
            // 对每一个必经节点执行操作，如果当前基本块的后续节点是该必经节点的话，那么就找到了一条回边
            for (auto succ_block = block->succ_begin(); succ_block != block->succ_end(); succ_block++)
            {
                if (*succ_block == domBB)
                {
                    std::pair<BasicBlock *, BasicBlock *> edge(block, domBB);
                    BackEdges.push_back(edge);
                    // std::cout<<block->getNo()<<"->"<<domBB->getNo()<<std::endl;
                }
            }
        }
    }
    return BackEdges;
}

void LoopCodeMotion::printBackEdges(std::vector<std::pair<BasicBlock *, BasicBlock *>> BackEdges)
{
    std::cout << "backedges:" << std::endl;
    for (auto edges : BackEdges)
    {
        std::cout << edges.first->getNo() << "->" << edges.second->getNo() << std::endl;
    }
}

// 我们已经有了回边的集合，现在我们想要获取回边组，要求每个组中的回边的到达节点是同一个
std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> LoopCodeMotion::mergeEdge(std::vector<std::pair<BasicBlock *, BasicBlock *>> &BackEdges)
{
    std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> edgeGroups;
    // 遍历每一条回边
    for (auto edge : BackEdges)
    {
        // std::cout<<edge.first->getNo()<<"->"<<edge.second->getNo()<<std::endl;
        // 一开始edgeGroups为空，我们先压入当前第一条回边构成的单独的vector
        if (edgeGroups.size() == 0)
        {
            std::vector<std::pair<BasicBlock *, BasicBlock *>> tempgroup;
            tempgroup.push_back(edge);
            edgeGroups.push_back(tempgroup);
        }
        else
        {
            bool find_group = false;
            // 遍历当前的每个组，如果当前这条回边的到达节点能够在已有组中找到就压入
            for (auto group : edgeGroups)
            {
                // std::cout<<"group[0].second->getNo():"<<group[0].second->getNo()<<std::endl;
                // std::cout<<group[0].second<<" "<<edge.second<<std::endl;
                if (group[0].second == edge.second)
                {
                    // std::cout<<"equal"<<std::endl;
                    // group.push_back(edge);
                    // for(auto e:group) std::cout<<e.first->getNo()<<"->"<<e.second->getNo()<<std::endl;
                    // 如果直接用上面的push_back会发现，虽然在这里面传进去了，但在外层根本看不见,不得已换用下面的
                    std::vector<std::pair<BasicBlock *, BasicBlock *>> tempgroup;
                    tempgroup.assign(group.begin(), group.end());
                    tempgroup.push_back(edge);
                    edgeGroups.push_back(tempgroup);
                    edgeGroups.erase(remove(edgeGroups.begin(), edgeGroups.end(), group), edgeGroups.end());
                    find_group = true;
                    break;
                }
            }
            // 否则就新创一个回边组
            if (!find_group)
            {
                std::vector<std::pair<BasicBlock *, BasicBlock *>> tempgroup;
                tempgroup.push_back(edge);
                edgeGroups.push_back(tempgroup);
            }
        }
        // printEdgeGroups(edgeGroups);
    }
    return edgeGroups;
}

void LoopCodeMotion::printEdgeGroups(std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> edgeGroups)
{
    for (auto group : edgeGroups)
    {
        std::cout << "group size: " << group.size() << std::endl;
        for (auto edge : group)
        {
            std::cout << edge.first->getNo() << "->" << edge.second->getNo() << std::endl;
        }
    }
}

// 获取循环，每一个循环结构体由基本块的vector组成（若i->j是一条回边，那么i，j以及所有不经过j能到达i的节点构成循环，j为循环首节点）
// 如果两个循环的首节点相同，那么我们合并循环（因此要先找出所有到达节点相同的回边组）
std::vector<std::vector<BasicBlock *>> LoopCodeMotion::calculateLoopList(Function *func, std::vector<std::vector<std::pair<BasicBlock *, BasicBlock *>>> &edgeGroups)
{
    std::vector<std::vector<BasicBlock *>> LoopList;

    // 遍历每一个回边的组，每个组生成一个循环loop
    for (auto group : edgeGroups)
    {
        std::vector<BasicBlock *> Loop;
        // headBlock为当前回边组的目标基本块，它一定在循环中
        BasicBlock *headBlock = group[0].second;
        Loop.push_back(headBlock);

        // 遍历当前组中的每一个回边
        for (auto edge : group)
        {
            // Lastadd存放回边的出发节点->该出发节点的父节点->父节点的父节点
            std::vector<BasicBlock *> Lastadd;
            // 如果当前回边的出发节点还没有被加入到循环中，那么就加入它
            // 如果已经加入了，就跳过，直接处理下一条边
            if (!count(Loop.begin(), Loop.end(), edge.first))
            {
                Loop.push_back(edge.first);
                Lastadd.push_back(edge.first);
            }
            else
            {
                continue;
            }

            // 能走到这里一定是当前这条回边的出发节点第一次被加入到Loop以及Lastadd
            // 在一开始，lastadd就比loop少了一个headBlock，这是为了规避对回边i->j,能到达i的节点又经过了j
            // 先把当前回边的出发节点的父节点全部加入到loop，再继续找这批父节点的父节点，直到不再添加新节点
            while (true)
            {
                std::vector<BasicBlock *> tempadd;
                bool ifAddNewBB = false;
                // 遍历lastadd的每一个节点，如果他们的父节点不在循环中，就加入tempadd
                for (auto block : Lastadd)
                {
                    for (auto fartherBB = block->pred_begin(); fartherBB != block->pred_end(); fartherBB++)
                    {
                        // 使用count避免加入重复的边
                        if (!count(Loop.begin(), Loop.end(), *fartherBB))
                        {
                            ifAddNewBB = true;
                            Loop.push_back(*fartherBB);
                            tempadd.push_back(*fartherBB);
                        }
                    }
                }
                if (!ifAddNewBB)
                    break;
                // assign：将区间[first,last)的元素赋值到当前的vector容器中，这个容器会清除掉vector容器中以前的内容
                Lastadd.assign(tempadd.begin(), tempadd.end());
            }
        }
        LoopList.push_back(Loop);
    }
    return LoopList;
}

void LoopCodeMotion::printLoop(std::vector<std::vector<BasicBlock *>> &LoopList)
{
    for (auto Loop : LoopList)
    {
        std::cout << "Loop size:" << Loop.size() << std::endl;
        for (auto block : Loop)
            std::cout << block->getNo() << " ";
        std::cout << std::endl;
    }
}

// 代码外提
void LoopCodeMotion::CodePullUp(Function *func, std::vector<std::vector<BasicBlock *>> &LoopList, std::vector<std::pair<BasicBlock *, BasicBlock *>> &BackEdges)
{
    // 获取必经节点的集合
    std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> domBBSet = getDomBBSet(func);
    // 遍历每一个循环
    for (auto Loop : LoopList)
    {
        // std::cout<<"Loop:"<<std::endl;
        // 获取出口节点的集合outBlock
        std::vector<BasicBlock *> outBlock = calculateOutBlock(Loop);
        // std::cout<<"outBlock:"<<std::endl;
        // for(auto block1:outBlock) std::cout<<block1->getNo()<<" "<<std::endl;

        // 计算循环不变的指令信息
        std::vector<Instruction *> LoopConstInstructions = calculateLoopConstant(Loop, func);
        // std::cout<<LoopConstInstructions.size()<<std::endl;
        // printLoopConst(LoopConstInstructions);
        //  for(auto op:LoopConst[func][Loop]) std::cout<<op->toStr()<<" ";
        //  std::cout<<std::endl;

        // headBlock为该循环的首节点
        BasicBlock *headBlock = Loop[0];
        // 新增的前继节点predBB
        BasicBlock *predBB = new BasicBlock(func);
        // 标志着是否是第一条外提的指令
        bool ifAddFirst = true;

        for (auto ins : LoopConstInstructions)
        {
            // 获取循环不变指令所在的节点fartherBB
            BasicBlock *fartherBB = ins->getParent();
            // 要求1： farther BB 为所有出口节点的支配节点
            bool if_DomBBAll = true;
            // 遍历所有出口节点去尝试
            for (auto block : outBlock)
            {
                std::vector<BasicBlock *> domBBList = domBBSet[block];
                if (!count(domBBList.begin(), domBBList.end(), fartherBB))
                {
                    if_DomBBAll = false;
                    break;
                }
            }
            // mem2reg优化以后，后面两个条件无需考虑
            // 死代码删除优化以后，无需考虑无用定值指令的外提，因为直接删掉了

            if (if_DomBBAll)
            {
                // 设置predBB的跳转，以及BasciBlock中的跳转指令的目的块
                // 只有是第一条要外提的指令我们才做处理，才去插入这个preBB
                if (ifAddFirst)
                {
                    ifAddFirst = false;
                    std::vector<BasicBlock *> pre_block_delete;

                    // 查headblock前驱，将其最后一条指令然后改掉，指向predBB
                    // 遍历该循环的首节点的所有前继节点
                    for (auto block = headBlock->pred_begin(); block != headBlock->pred_end(); block++)
                    {
                        // 只用改不是回边的块的前驱后继，为什么？
                        std::pair<BasicBlock *, BasicBlock *> edge(*block, headBlock);
                        if (count(BackEdges.begin(), BackEdges.end(), edge))
                        {
                            continue;
                        }

                        pre_block_delete.push_back(*block);
                        // 获取该前继基本块的最后一条指令
                        Instruction *lastins = (*block)->rbegin();
                        // 如果是条件跳转
                        if (lastins->isCond())
                        {
                            CondBrInstruction *last = (CondBrInstruction *)lastins;
                            // headBlock有可能是从真分支跳来的or假分支跳来的
                            if (last->getTrueBranch() == headBlock)
                            {
                                last->setTrueBranch(predBB);
                            }
                            else if (last->getFalseBranch() == headBlock)
                            {
                                last->setFalseBranch(predBB);
                            }
                        }
                        // 如果是非条件跳转
                        else if (lastins->isUncond())
                        {
                            UncondBrInstruction *last = (UncondBrInstruction *)lastins;
                            last->setBranch(predBB);
                        }
                        (*block)->removeSucc(headBlock);
                        (*block)->addSucc(predBB);
                        predBB->addPred(*block);
                    }

                    headBlock->addPred(predBB);
                    predBB->addSucc(headBlock);
                    new UncondBrInstruction(headBlock, predBB);
                    for (auto block : pre_block_delete)
                    {
                        // std::cout<<block->getNo()<<std::endl;
                        headBlock->removePred(block);
                    }
                    // phi指令,这个处理
                    changePhiInstruction(Loop, predBB, pre_block_delete);
                }

                // 更改这条要外提的指令与它周边指令间的关系
                Instruction *previns = ins->getPrev();
                Instruction *nextins = ins->getNext();

                previns->setNext(nextins);
                nextins->setPrev(previns);

                ins->setParent(predBB);
                // 将ins插在predBB->rbegin()的前面
                predBB->insertBefore(ins, predBB->rbegin());
            }
        }
    }
}

// 查找出口节点
// 出口结点是指循环中具有如下性质的结点：从该结点有一条有向边引到循环外的某结点
std::vector<BasicBlock *> LoopCodeMotion::calculateOutBlock(std::vector<BasicBlock *> &Loop)
{
    std::vector<BasicBlock *> outBlock;
    for (auto block : Loop)
    {
        // 查找后继节点看是否在循环里，只要有一个在循环外，就可以加入
        for (auto succBB = block->succ_begin(); succBB != block->succ_end(); succBB++)
        {
            if (!count(Loop.begin(), Loop.end(), *succBB))
            {
                outBlock.push_back(block);
                break; // 要不要break？？？
            }
        }
    }
    return outBlock;
}

void LoopCodeMotion::printLoopConst(std::vector<Instruction *> LoopConstInstructions)
{
    for (auto ins : LoopConstInstructions)
    {
        ins->output();
    }
}

// 计算循环不变信息，返回指令的向量集合,同时将不变的操作数存入LoopConst
// 循环不变运算可以是一元/二元/赋值/等等，这边具体参考我们已经有的所有可能的中间代码指令类型，需要仔细设计
std::vector<Instruction *> LoopCodeMotion::calculateLoopConstant(std::vector<BasicBlock *> Loop, Function *func)
{
    loopStoreOperands.clear();
    for (auto block : Loop)
    {
        for (auto ins = block->begin(); ins != block->end(); ins = ins->getNext())
        {
            if (ins->isStore())
            {
                loopStoreOperands.insert(ins->getUse()[0]);
            }
        }
    }

    std::vector<Instruction *> LoopConstInstructions;
    std::set<Operand *> Operands; // 避免重复
    // 我们只能够保证，加入了loopConst的那些操作数，就代表了在循环中至少存在它的一条定值语句被标记为循环不变指令
    LoopConst[func][Loop] = Operands;
    // 遍历当前循环的每一个基本块,一次遍历完成之后，去判断有没有新增外提指令，有就继续
    while (true)
    {
        bool ifAddNew = false;
        for (auto block : Loop)
        {
            Instruction *ins = block->begin();
            Instruction *last = block->end();
            // 遍历这个基本块的所有指令，while循环中ins每次在最后面“+1”
            while (ins != last)
            {
                // 如果当前的指令它没有存入LoopConstInstructions，我们才去讨论它，否则略去
                if (!count(LoopConstInstructions.begin(), LoopConstInstructions.end(), ins))
                {
                    // 如果指令类型是二元
                    if (ins->isBinary())
                    {
                        // 获取当前指令的use
                        std::vector<Operand *> useOperands = ins->getUse();
                        int constant_count = 0;

                        // 遍历所有的use操作数
                        for (auto operand : useOperands)
                        {
                            // 获取它们的类型
                            Type *operand_type = operand->getEntry()->getType();
                            // 如果是int
                            if (operand_type->isInt())
                            {
                                // 是常量
                                if (operand->getEntry()->isConstant())
                                {
                                    // 插入LoopConst
                                    LoopConst[func][Loop].insert(operand);
                                    constant_count++;
                                }
                                // 对该操作数的定值运算全部被标记为循环不变
                                else if (OperandIsLoopConst(operand, Loop, LoopConstInstructions))
                                {
                                    LoopConst[func][Loop].insert(operand);
                                    constant_count++;
                                }
                            }
                            // 如果是float
                            else if (operand_type->isFloat())
                            {
                                if (operand->getEntry()->isConstant())
                                {
                                    LoopConst[func][Loop].insert(operand);
                                    constant_count++;
                                }
                                // 对该操作数的定值运算全部被标记为循环不变
                                else if (OperandIsLoopConst(operand, Loop, LoopConstInstructions))
                                {
                                    LoopConst[func][Loop].insert(operand);
                                    constant_count++;
                                }
                            }
                        }
                        // 二元运算，需要两个use操作数都是循环不变量，那么这条指令就是循环不变运算
                        if (constant_count == 2)
                        {
                            // ins->output();
                            LoopConstInstructions.push_back(ins);
                            ifAddNew = true;
                        }
                    }
                    // 如果是F2I或I2F
                    if (ins->isFICal())
                    {
                        Operand *operand = ins->getUse()[0];
                        if (operand->getEntry()->isConstant() ||
                            OperandIsLoopConst(operand, Loop, LoopConstInstructions))
                        {
                            LoopConst[func][Loop].insert(operand);
                            LoopConstInstructions.push_back(ins);
                            ifAddNew = true;
                        }
                    }
                    // 如果指令类型为alloc,无条件接受，因为他只可能是循环中的数组定义
                    else if (ins->isAlloc())
                    {
                        LoopConstInstructions.push_back(ins);
                        LoopConst[func][Loop].insert(ins->getDef());
                        ifAddNew = true;
                    }
                    else if (ins->isGep())
                    {
                        std::vector<Operand *> useOperands = ins->getUse();
                        int constant_count = 0;
                        for (auto useOp : useOperands)
                        {
                            Type *operand_type = useOp->getEntry()->getType();
                            if (operand_type->isInt())
                            {
                                if (useOp->getEntry()->isConstant())
                                {
                                    LoopConst[func][Loop].insert(useOp);
                                    constant_count++;
                                }
                                // 对该操作数的定值运算全部被标记为循环不变
                                else if (OperandIsLoopConst(useOp, Loop, LoopConstInstructions))
                                {
                                    LoopConst[func][Loop].insert(useOp);
                                    constant_count++;
                                }
                            }
                            else if (operand_type->isPtr())
                            {
                                // PointerType* operand_ptrtype=(PointerType*)operand_type;
                                if (useOp->isGlobal())
                                {
                                    LoopConst[func][Loop].insert(useOp);
                                    constant_count++;
                                }
                                // 对该操作数的定值运算全部被标记为循环不变
                                else if (OperandIsLoopConst(useOp, Loop, LoopConstInstructions, ins))
                                {
                                    LoopConst[func][Loop].insert(useOp);
                                    constant_count++;
                                }
                            }
                        }
                        if (constant_count == useOperands.size())
                        {
                            // ins->output();
                            LoopConstInstructions.push_back(ins);
                            ifAddNew = true;
                        }
                    }
                    else if (ins->isBitcast())
                    {
                        Operand *useOp = ins->getUse()[0];
                        int constant_count = 0;
                        Type *operand_type = useOp->getEntry()->getType();
                        if (operand_type->isPtr())
                        {
                            // PointerType* operand_ptrtype=(PointerType*)operand_type;
                            if (useOp->isGlobal())
                            {
                                LoopConst[func][Loop].insert(useOp);
                                constant_count++;
                            }
                            // 对该操作数的定值运算全部被标记为循环不变
                            else if (OperandIsLoopConst(useOp, Loop, LoopConstInstructions))
                            {
                                LoopConst[func][Loop].insert(useOp);
                                constant_count++;
                            }
                        }
                        if (constant_count == 1)
                        {
                            // ins->output();
                            LoopConstInstructions.push_back(ins);
                            ifAddNew = true;
                        }
                    }
                    else if (ins->isCall())
                    {
                        auto funcSE = (IdentifierSymbolEntry *)(((CallInstruction *)ins)->getFunc());
                        if (funcSE->getName() == "llvm.memset.p0i8.i32")
                        {
                            std::vector<Operand *> useOperands = ins->getUse();
                            Operand *firstOp = useOperands[0];
                            Operand *thirdOp = useOperands[2];
                            if (OperandIsLoopConst(firstOp, Loop, LoopConstInstructions) &&
                                (OperandIsLoopConst(thirdOp, Loop, LoopConstInstructions) || thirdOp->getEntry()->isConstant()))
                            {
                                LoopConstInstructions.push_back(ins);
                                ifAddNew = true;
                            }
                        }
                    }
                    else if (ins->isStore())
                    {
                        std::vector<Operand *> useOperands = ins->getUse();
                        Operand *addrOp = useOperands[0];
                        Operand *valueOp = useOperands[1];
                        int constant_count = 0;
                        if (addrOp->isGlobal() || OperandIsLoopConst(addrOp, Loop, LoopConstInstructions))
                        {
                            // std::cout<<"is global:"<<addrOp->toStr()<<std::endl;
                            constant_count++;
                        }
                        if (valueOp->getEntry()->isConstant() || OperandIsLoopConst(valueOp, Loop, LoopConstInstructions))
                        {
                            constant_count++;
                        }
                        if (constant_count == 2)
                        {
                            //面向样例加点特殊的判断
                            Operand* gepBaseDef=nullptr;
                            if(addrOp->getDef()&&addrOp->getDef()->isGep()){
                                Operand* gepBase=addrOp->getDef()->getUse()[0];
                                if(gepBase->getDef()&&gepBase->getDef()->isGep()){
                                    gepBaseDef=gepBase->getDef()->getUse()[0];
                                }
                            }
                            bool notPull=false;
                            if(gepBaseDef&&gepBaseDef->isGlobal()){
                                //向下找call指令，看看被调函数是否修改了gepBaseDef
                                //std::cout<<gepBaseDef->toStr();
                                for(auto temp=ins->getNext();temp!=ins->getParent()->end();temp=temp->getNext()){
                                    if(temp->isCall()){
                                        Function* func=((IdentifierSymbolEntry*)(((CallInstruction*)temp)->getFunc()))->getFunction();
                                        if(pureFunc1==nullptr){
                                            pureFunc1 = new PureFunctionAnalyser(func->getParent());
                                        }
                                        std::set<std::string> storeGlobalVar=pureFunc1->getStoreGlobalVar(func); 
                                        for(auto str1:storeGlobalVar){
                                            if(str1==gepBaseDef->toStr().substr(1)){
                                                notPull=true;
                                                break;
                                            }
                                        }
                                        if(notPull){
                                            break;
                                        }            
                                    }
                                }
                            }
                            if(!notPull){
                                LoopConstInstructions.push_back(ins);
                                // store 不增加新的def                                
                            }
                        }
                    }
                    // 根据数据流分析，load的def不能够影响到所在基本块的cmp语句中的任一use，否则一些判断会出错，造成死循环
                    else if (ins->isLoad())
                    {
                        // 判断这个load是否影响
                        if (!isLoadInfluential(ins))
                        {
                            // 判断是否循环不变
                            int constant_count = 0;
                            Operand *useOp = ins->getUse()[0];
                            Type *operand_type = useOp->getEntry()->getType();
                            if (operand_type->isPtr())
                            {
                                // PointerType* operand_ptrtype=(PointerType*)operand_type;
                                if (useOp->isGlobal())
                                {
                                    LoopConst[func][Loop].insert(useOp);
                                    constant_count++;
                                }
                                else if (OperandIsLoopConst(useOp, Loop, LoopConstInstructions))
                                {
                                    LoopConst[func][Loop].insert(useOp);
                                    constant_count++;
                                }
                            }
                            if (constant_count == 1)
                            {
                                LoopConstInstructions.push_back(ins);
                                ifAddNew = true;
                            }
                        }
                    }
                }
                ins = ins->getNext();
            }
        }
        if (!ifAddNew)
            break;
    }
    return LoopConstInstructions;
}

bool LoopCodeMotion::isLoadInfluential(Instruction *ins)
{
    // 考虑函数内联后，对样例37的特殊处理
    Operand *loadUse = ins->getUse()[0];
    for (auto use : loopStoreOperands)
    {
        if (use->toStr() == loadUse->toStr())
        {
            return true;
        }
    }
    Instruction *temp = ins->getNext();
    BasicBlock *block = ins->getParent();
    // 如果load一个全局的话，这个全局变量之前可能call函数，修改了这个全局变量,这样就不是一个不变指令了
    // 下面的处理是很粗糙的
    if (loadUse->isGlobal())
    {
        Instruction *temp1 = block->begin();
        while (temp1 != ins)
        {
            if (temp1->isCall())
            {
                Function* func=((IdentifierSymbolEntry*)(((CallInstruction*)temp1)->getFunc()))->getFunction();
                if(pureFunc1==nullptr){
                    pureFunc1 = new PureFunctionAnalyser(func->getParent());
                }
                std::set<std::string> storeGlobalVar=pureFunc1->getStoreGlobalVar(func); 
                for(auto str1:storeGlobalVar){
                    if(str1==loadUse->toStr().substr(1)){
                        return true;
                    }
                }
            }
            temp1 = temp1->getNext();
        }
    }

    std::vector<Operand *> affectedOperands;
    affectedOperands.push_back(ins->getDef());
    // 函数内联后出现了一个问题，有的基本块莫名被断成两半，因此
    // 如果一个基本块，以直接跳转结尾，就继续跳到那个基本块处理
    // 直到遇到一个cmp语句+分支跳转的基本块停下
    while (true)
    {
        while (temp != block->end())
        {
            // 默认store有影响，因为涉及到全局/数组的存取，这二者每次使用都需要重新load，中间变量不同，无法通过中间变量来判断
            // if(temp->isStore()){return true;}
            // 如果当前这条指令的use存在于affectedOperands，那么他的def就受影响
            for (auto use : temp->getUse())
            {
                if (count(affectedOperands.begin(), affectedOperands.end(), use))
                {
                    // 如果遇到cmp，要求不能有被影响的操作数,否则load就是有影响的
                    // 如果遇到受影响的store，那么也返回true
                    // 如果遇到call，参数中含有受影响的，也直接返回true
                    if (temp->isCmp() || temp->isStore() || temp->isCall())
                    {
                        return true;
                    }
                    affectedOperands.push_back(temp->getDef());
                    break;
                }
            }
            temp = temp->getNext();
        }
        // 如果是无条件跳转的话,更新
        Instruction *last = temp->getPrev();
        if (last->isUncond())
        {
            block = ((UncondBrInstruction *)last)->getBranch();
            temp = block->begin();
        }
        else
        {
            break;
        }
    }
    return false;
}

// 但凡在循环中存在一条对该操作数的定值语句，它没有被标记为循环不变语句，就不通过
// 若定值全在外，或有在里面，但都被标记，则true
// 现在全部的依凭就是LoopConstInstructions
bool LoopCodeMotion::OperandIsLoopConst(Operand *op, std::vector<BasicBlock *> Loop, std::vector<Instruction *> LoopConstInstructions, Instruction *gepIns)
{
    for (auto block : Loop)
    {
        Instruction *i = block->begin();
        Instruction *last = block->end();
        while (i != last)
        {
            // if(i->getDef())std::cout<<op->toStr()<<":"<<i->getDef()->toStr()<<std::endl;
            // 无需考虑store,它只有getUse，所以下面if进不去
            // 这是因为store存值，后面必定是用load取出同一个位置的值，而不会复用同一个中间变量
            // 在循环中找到了一条定值语句,mem2reg优化之下，它只可能有一个定值
            if (i->getDef() == op)
            {
                // 如果这条定值语句目前不是循环不变语句，那就不通过
                if (!count(LoopConstInstructions.begin(), LoopConstInstructions.end(), i))
                {
                    return false;
                }
                else
                {
                    return true;
                }
            }
            // 考虑数组在循环中，另外被load然后store赋值的情况
            if (gepIns && i->isGep())
            {
                Operand* gepDef=gepIns->getDef();
                bool needConsider=true;
                if (i->getDef()->toStr() != gepDef->toStr() && i->getUse().size()==gepIns->getUse().size())
                {
                    //细化判断，要求每一个偏移都一样
                    for(auto pos=0;pos<i->getUse().size();pos++){
                        if(i->getUse()[pos]->toStr()!=gepIns->getUse()[pos]->toStr()){
                            needConsider=false;
                            break;
                        }
                    }
                    if(needConsider){
                        // 下面就只是处理一维的情况
                        Operand *def = i->getDef();
                        Instruction *temp = i;
                        while (temp != last)
                        {
                            if (temp->isStore() && temp->getUse()[0] == def)
                            {
                                return false;
                            }
                            temp = temp->getNext();
                        }                       
                    }
                }
            }
            i = i->getNext();
        }
    }
    return true;
}

// phi指令的块不在循环里，改成对应的preblock
void LoopCodeMotion::changePhiInstruction(std::vector<BasicBlock *> &Loop, BasicBlock *newPreBlock, std::vector<BasicBlock *> oldBlocks)
{
    for (auto block : Loop)
    {
        Instruction *i = block->begin();
        Instruction *last = block->end();
        while (i != last)
        {
            // 是phi指令
            if (i->isPhi())
            {
                PhiInstruction *pi = (PhiInstruction *)i;
                // 判断phi指令的src块是否在循环里
                for (auto oldBlock : oldBlocks)
                {
                    if (pi->findSrc(oldBlock))
                    {
                        Operand *op = pi->getBlockSrc(oldBlock);
                        pi->removeBlockSrc(oldBlock);
                        pi->addSrc(newPreBlock, op);
                    }
                }
            }
            i = i->getNext();
        }
    }
}

void LoopCodeMotion::dealwithNoPreBB(Function *func)
{
    std::vector<BasicBlock *> temp;
    std::vector<BasicBlock *> blocklist = func->getBlockList();
    for (auto it = blocklist.begin(); it != blocklist.end(); it++)
    {
        if ((*it)->getNumOfPred() == 0 && *it != func->getEntry())
        {
            for (auto it1 = (*it)->pred_begin(); it1 != (*it)->pred_end(); it1++)
            {
                (*it1)->removePred(*it);
            }
            temp.push_back(*it);
            // ret = true;
        }
    }
    for (auto b : temp)
        func->remove(b);
}

#include "LoopOptimization.h"
#include <algorithm>

//目前只完成代码外提的优化，还可以加强度削弱、循环展开
void LoopOptimization::pass(){
    //遍历每一个函数做操作
    for(auto func=unit->begin();func!=unit->end();func++){
        //计算当前函数每一个基本块的必经节点，存入DomBBSet
        calculateFinalDomBBSet(*func);
        printDomBB(*func);
        
        //计算当前函数的回边集合
        std::vector<std::pair<BasicBlock*,BasicBlock*>> BackEdges=getBackEdges(*func);
        //查找当前函数的循环体的集合
        std::vector<std::vector<BasicBlock*> > LoopList=calculateLoopList(*func,BackEdges);
        printLoop(LoopList);
        
        //代码外提
        CodePullUp(*func,LoopList,BackEdges);
        /*
        //下面这个函数是用来干嘛的？？？莫名诡异，看起来是和代码外提相绑定的
        //dealwithNoPreBB(*func);
        
        
        //外提后会产生新的块 需要重新计算循环
        calculateFinalDomBBSet(*func);
        BackEdges=getBackEdges(*func);
        LoopList=calculateLoopList(*func,BackEdges);
        */
    }
}

//计算当前函数每一个基本块的必经节点，存入DomBBSet
void LoopOptimization::calculateFinalDomBBSet(Function* func){
    //初始化当前函数的domBBMap，入口基本块设它的必经节点为本身，其他基本块设他的必经节点为全部基本块的集合
    initializeDomBBSet(func);
    //获取DomBBSet[func],存入lastSet，用于比较
    std::unordered_map<BasicBlock*,std::vector<BasicBlock*>> lastSet(getDomBBSet(func));
    
    while(true){
        //获取DomBBSet[func],存入DomSet，我们每一轮迭代在DomSet上修改
        std::unordered_map<BasicBlock*,std::vector<BasicBlock*>>DomSet(getDomBBSet(func));
        
        //遍历当前函数每一个基本块，获取他们的必经节点
        for(auto block: func->getBlockList()){
            std::vector<BasicBlock*> finalDomList;
            bool ifFirst=true;
            //遍历当前基本块的每一个前继
            //如果只有一个前继基本块，那么前继基本块的必经节点一定是当前基本块的必经节点
            //如果有多个前继基本块，那么那些前继基本块共有的必经节点一定是当前基本块的必经节点
            for (auto fartherBlock=block->pred_begin();fartherBlock!=block->pred_end();fartherBlock++){
                if(ifFirst){
                    finalDomList=DomSet[*fartherBlock];
                    ifFirst=false;
                    continue;
                }
                finalDomList=getIntersectBBList(DomSet[*fartherBlock],finalDomList);
            }
            //当前节点的支配节点集合必定包括它自身
            if(!count(finalDomList.begin(),finalDomList.end(),block)){
                finalDomList.push_back(block);
            }
            DomSet[block]=finalDomList;
        }

        //与迭代前的那一版DomBBSet做比较，如果相同，就代表迭代完成，否则继续迭代
        DomBBSet[func]=DomSet;
        if(!ifDomBBSetChange(lastSet,func)){
            break;
        }
        else{
            lastSet=getDomBBSet(func);
        }
    }
    return;
}

//对当前函数初始化DomBBSet
void LoopOptimization :: initializeDomBBSet(Function* func){
    //如果当前的函数还没有计算domBBMap的话，初始化一个给它
    if(DomBBSet.find(func)==DomBBSet.end()){
        std::unordered_map<BasicBlock*,std::vector<BasicBlock*>>domBBMap;
        DomBBSet[func]=domBBMap;
    }
    //遍历当前函数的每一个基本块
    for (auto block : func->getBlockList()) {
        //如果当前函数的当前基本块还没有计算必经节点
        if(DomBBSet[func].find(block)==DomBBSet[func].end()){
            //如果当前基本块是函数的入口点
            if(block==func->getEntry()){
                std::vector<BasicBlock*> dom_list;
                dom_list.push_back(func->getEntry());
                //入口基本块的必经节点只有它自己（只支配它自己）
                DomBBSet[func][block]=dom_list;
            }
            else{
                //如果不是入口基本块，就先设必经节点是当前函数全部基本块的集合
                std::vector<BasicBlock*> dom_list(func->getBlockList());
                DomBBSet[func][block]=dom_list;
            }
        }
    }
    return;
}

//找出两个序列中共有的那些个节点
std::vector<BasicBlock*> LoopOptimization::getIntersectBBList(std::vector<BasicBlock*>& List1,std::vector<BasicBlock*>& List2){
    std::vector<BasicBlock*> finalList;
    for (auto block: List1){
        if(count(List2.begin(), List2.end(), block)){
            finalList.push_back(block);
        }
    }
    return finalList;
}

//比较DomBBSet是否发生了改变
bool LoopOptimization :: ifDomBBSetChange(std::unordered_map<BasicBlock*,std::vector<BasicBlock*>>& lastSet,Function* func){
    std::unordered_map<BasicBlock*,std::vector<BasicBlock*>> DomBB_map=getDomBBSet(func);
    for(auto block: func->getBlockList()){
        if(lastSet[block]!=DomBB_map[block]){
            return true;
        }
    }
    return false;
}

void LoopOptimization::printDomBB(Function *func){
    std::cout<<"domBB:"<<std::endl;
    for(auto block:(func)->getBlockList()){
        std::cout<<block->getNo()<<":";
        for(auto dom : DomBBSet[func][block]){
            std::cout<<dom->getNo()<<" ";
        }
        std::cout<<std::endl;
    }
}

//获取回边（如果存在从节点i到j的有向边，并且j是i的一个必经节点，那么这条边就是回边）
std::vector<std::pair<BasicBlock*,BasicBlock*>> LoopOptimization::getBackEdges(Function* func){
    //先获取DomBBSet
    std::unordered_map<BasicBlock*,std::vector<BasicBlock*>> DomSet=getDomBBSet(func);
    std::vector<std::pair<BasicBlock*,BasicBlock*>> BackEdges;
    //search for backedges
    std::cout<<"backedges:"<<std::endl;
    //遍历当前基本块的每一个必经节点
    for (auto block:func->getBlockList()){
        for (auto domBB:DomSet[block]){
            //对每一个必经节点执行操作，如果当前基本块的后续节点是该必经节点的话，那么就找到了一条回边
            for(auto succ_block=block->succ_begin();succ_block<block->succ_end();succ_block++){
                if(*succ_block==domBB){
                    std::pair<BasicBlock*,BasicBlock*> edge(block,domBB);
                    BackEdges.push_back(edge);
                    std::cout<<block->getNo()<<"->"<<domBB->getNo()<<std::endl;
                }
            }
        }
    }
    return BackEdges;
}

//获取循环，每一个循环结构体由基本块的vector组成（若i->j是一条回边，那么i，j以及所有不经过j能到达i的节点构成循环，j为循环首节点）
//如果两个循环的首节点相同，那么我们合并循环（因此要先找出所有到达节点相同的回边组）
std::vector<std::vector<BasicBlock*>> LoopOptimization::calculateLoopList(Function* func,std::vector<std::pair<BasicBlock*,BasicBlock*>>& BackEdges){
    std::vector<std::vector<BasicBlock*>> LoopList;
    //获取回边组，要求每个组中的回边的到达节点是同一个
    std::vector<std::vector<std::pair<BasicBlock*,BasicBlock*>>> edgeGroups=mergeEdge(BackEdges);
    //search for natural loop

    printEdgeGroups(edgeGroups);

    //遍历每一个回边的组，每个组生成一个循环loop
    for(auto group:edgeGroups){
        std::vector<BasicBlock*> Loop;
        //headBlock为当前回边组的目标基本块，它一定在循环中
        BasicBlock* headBlock=group[0].second;
        Loop.push_back(headBlock);
        
        //遍历当前组中的每一个回边
        for (auto edge:group){
            //Lastadd存放回边的出发节点->该出发节点的父节点->父节点的父节点
            std::vector<BasicBlock*> Lastadd;   
            //如果当前回边的出发节点还没有被加入到循环中，那么就加入它
            //如果已经加入了，就跳过，直接处理下一条边   
            if(!count(Loop.begin(),Loop.end(),edge.first)){
                Loop.push_back(edge.first);
                Lastadd.push_back(edge.first);
            }
            else{
                continue;
            }
            
            //能走到这里一定是当前这条回边的出发节点第一次被加入到Loop以及Lastadd
            //在一开始，lastadd就比loop少了一个headBlock，这是为了规避对回边i->j,能到达i的节点又经过了j
            //先把当前回边的出发节点的父节点全部加入到loop，再继续找这批父节点的父节点，直到不再添加新节点
            while(true){
                std::vector<BasicBlock*> tempadd;
                bool ifAddNewBB=false;
                //遍历lastadd的每一个节点，如果他们的父节点不在循环中，就加入tempadd
                for(auto block:Lastadd){
                    for(auto fartherBB=block->pred_begin();fartherBB!=block->pred_end();fartherBB++){
                        //使用count避免加入重复的边
                        if(!count(Loop.begin(),Loop.end(),*fartherBB)){
                            ifAddNewBB=true;
                            Loop.push_back(*fartherBB);
                            tempadd.push_back(*fartherBB);
                        }
                    }         
                }
                if(!ifAddNewBB)
                    break;
                //assign：将区间[first,last)的元素赋值到当前的vector容器中，这个容器会清除掉vector容器中以前的内容
                Lastadd.assign(tempadd.begin(),tempadd.end());
            }
        }
        LoopList.push_back(Loop);
    }
    return LoopList;
}

//我们已经有了回边的集合，现在我们想要获取回边组，要求每个组中的回边的到达节点是同一个
std::vector<std::vector<std::pair<BasicBlock*,BasicBlock*>>> LoopOptimization::mergeEdge(std::vector<std::pair<BasicBlock*,BasicBlock*>>& BackEdges){
    std::vector<std::vector<std::pair<BasicBlock*,BasicBlock*>>> edgeGroups;
    //遍历每一条回边
    for(auto edge:BackEdges){
        //std::cout<<edge.first->getNo()<<"->"<<edge.second->getNo()<<std::endl;
        //一开始edgeGroups为空，我们先压入当前第一条回边构成的单独的vector
        if(edgeGroups.size()==0){
            std::vector<std::pair<BasicBlock*,BasicBlock*>> tempgroup;
            tempgroup.push_back(edge);
            edgeGroups.push_back(tempgroup);
        }
        else{
            bool find_group=false;
            //遍历当前的每个组，如果当前这条回边的到达节点能够在已有组中找到就压入
            for(auto group:edgeGroups){
                //std::cout<<"group[0].second->getNo():"<<group[0].second->getNo()<<std::endl;
                //std::cout<<group[0].second<<" "<<edge.second<<std::endl;
                if(group[0].second==edge.second){
                    //std::cout<<"equal"<<std::endl;
                    //group.push_back(edge);
                    //for(auto e:group) std::cout<<e.first->getNo()<<"->"<<e.second->getNo()<<std::endl;
                    //如果直接用上面的push_back会发现，虽然在这里面传进去了，但在外层根本看不见,不得已换用下面的
                    std::vector<std::pair<BasicBlock*,BasicBlock*>> tempgroup;
                    tempgroup.assign(group.begin(),group.end());
                    tempgroup.push_back(edge);
                    edgeGroups.push_back(tempgroup);
                    //这真的不会报错吗？？？感觉严重拖慢了速度
                    edgeGroups.erase(remove(edgeGroups.begin(),edgeGroups.end(),group),edgeGroups.end());
                    find_group=true;
                    break;
                }
            }
            //否则就新创一个回边组
            if(!find_group){
                std::vector<std::pair<BasicBlock*,BasicBlock*>> tempgroup;
                tempgroup.push_back(edge);
                edgeGroups.push_back(tempgroup);
            }
        }
    //printEdgeGroups(edgeGroups);
    }
    return edgeGroups;
}

void LoopOptimization::printEdgeGroups(std::vector<std::vector<std::pair<BasicBlock*,BasicBlock*>>> edgeGroups){
    for(auto group:edgeGroups){
        std::cout<<"group size: "<<group.size()<<std::endl;
        for(auto edge:group){
            std::cout<<edge.first->getNo()<<"->"<<edge.second->getNo()<<std::endl;
        }
    }   
}

void LoopOptimization::printLoop(std::vector<std::vector<BasicBlock*>>& LoopList){
    for(auto Loop:LoopList){
        std::cout<<"Loop size:"<<Loop.size()<<std::endl;
        for(auto block:Loop)
            std::cout<<block->getNo()<<" ";
        std::cout<<std::endl;
    }
}

//代码外提
void LoopOptimization::CodePullUp(Function* func,std::vector<std::vector<BasicBlock*>>& LoopList, std::vector<std::pair<BasicBlock*,BasicBlock*>>& BackEdges){
    //获取必经节点的集合
    std::unordered_map<BasicBlock*,std::vector<BasicBlock*>> domBBSet=getDomBBSet(func);
    // 遍历每一个循环
    for(auto Loop:LoopList){
        std::cout<<"Loop:"<<std::endl;
        //获取出口节点的集合outBlock
        std::vector<BasicBlock*> outBlock=calculateOutBlock(Loop);
        std::cout<<"outBlock:"<<std::endl;  
        for(auto block1:outBlock) std::cout<<block1->getNo()<<" "<<std::endl;
        
        //计算循环不变的指令信息
        std::vector<Instruction*> LoopConstInstructions=calculateLoopConstant(Loop,func);
        std::cout<<LoopConstInstructions.size()<<std::endl;
        //printLoopConst(LoopConstInstructions);
        //for(auto op:LoopConst[func][Loop]) std::cout<<op->toStr()<<std::endl;
        /*
        // headBlock为该循环的首节点
        BasicBlock * headBlock=Loop[0];
        // 新增的前继节点predBB
        BasicBlock * predBB = new BasicBlock(func);
        //标志着是否是第一条外提的指令
        bool ifAddFirst=true;

        for(auto ins:LoopConstInstructions){
            //获取循环不变指令所在的节点fartherBB
            BasicBlock* fartherBB=ins->getParent();
            //要求1： farther BB 为所有出口节点的支配节点
            bool if_DomBBAll=true;
            //遍历所有出口节点去尝试
            for(auto block:outBlock){
                std::vector<BasicBlock*> domBBList=domBBSet[block];
                if(!count(domBBList.begin(),domBBList.end(),fartherBB)){
                    if_DomBBAll=false;
                    break;
                }
            }
            // 可以外提吗？？？？
            // 还需要设计如下两个条件：
            // 条件2：A在循环L中的其他地方没有定值语句
            // 条件3：循环L中所有对于A的引用，只有s中对于A的定值能够到达
            
            if(if_DomBBAll){
                //设置predBB的跳转，以及BasciBlock中的跳转指令的目的块
                //只有是第一条要外提的指令我们才做处理，才去插入这个preBB
                if(ifAddFirst){
                    ifAddFirst=false;
                    std::vector<BasicBlock*> pre_block_delete;

                    //查headblock前驱，将其最后一条指令然后改掉，指向predBB
                    //是这样外提的吗？？
                    //遍历该循环的首节点的所有前继节点
                    for(auto block=headBlock->pred_begin();block!=headBlock->pred_end();block++){
                        //只用改不是回边的块的前驱后继，为什么？
                        std::pair<BasicBlock*,BasicBlock*> edge(*block,headBlock);
                        if(count(BackEdges.begin(), BackEdges.end(), edge)){
                            continue;
                        }

                        pre_block_delete.push_back(*block);
                        //获取该前继基本块的最后一条指令
                        Instruction* lastins=(*block)->rbegin();
                        //如果是条件跳转
                        if(lastins->isCond()){
                            CondBrInstruction* last =(CondBrInstruction*)lastins;
                            //headBlock有可能是从真分支跳来的or假分支跳来的
                            if(last->getTrueBranch()==headBlock){
                                last->setTrueBranch(predBB);
                            }
                            else if(last->getFalseBranch()==headBlock){    
                                last->setFalseBranch(predBB);
                            }
                        }
                        //如果是非条件跳转
                        else if(lastins->isUncond()){
                            UncondBrInstruction* last=(UncondBrInstruction*)lastins;
                            last->setBranch(predBB);
                        }
                        //最后一条指令有可能有其他的情况吗？？？？？？？？？？
                        (*block)->removeSucc(headBlock);
                        (*block)->addSucc(predBB);
                        predBB->addPred(*block);
                    }
                    
                    headBlock->addPred(predBB);
                    predBB->addSucc(headBlock);
                    new UncondBrInstruction(headBlock,predBB);
                    for(auto block:pre_block_delete){
                        // std::cout<<block->getNo()<<std::endl;
                        headBlock->removePred(block);
                    }
                    //phi指令又是什么指令呢？？？
                    changePhiInstruction(Loop,predBB,pre_block_delete);
                }
 
                //更改这条要外提的指令与它周边指令间的关系
                Instruction* previns=ins->getPrev();
                Instruction* nextins=ins->getNext();

                previns->setNext(nextins);
                nextins->setPrev(previns);

                ins->setParent(predBB);
                //将ins插在predBB->rbegin()的前面，一开始会有一条空指令在基本块中
                //这样子是越早插入的在越前面，但是，这样能够满足：若s的运算对象B或C是在L中定值的，那么B或C的定值代码要先插入吗？？？可能不行？
                predBB->insertBefore(ins,predBB->rbegin());
            }
        }
        */
    }
}

//查找出口节点（这里是不是有问题，会重复添加把？？？？？是不是要一个break？）
//出口结点是指循环中具有如下性质的结点：从该结点有一条有向边引到循环外的某结点
std::vector<BasicBlock*> LoopOptimization::calculateOutBlock(std::vector<BasicBlock*>& Loop){
    std::vector<BasicBlock*> outBlock;
    for(auto block:Loop){
        //查找后继节点看是否在循环里，只要有一个在循环外，就可以加入
        for(auto succBB=block->succ_begin();succBB!=block->succ_end();succBB++){
            if(!count(Loop.begin(),Loop.end(),*succBB)){
                outBlock.push_back(block);
                break;//要不要break？？？
            }
        }
    }
    return outBlock;
}

void LoopOptimization::printLoopConst(std::vector<Instruction*> LoopConstInstructions){
    for(auto ins:LoopConstInstructions){
        ins->output();
    }
}


//计算循环不变信息，返回指令的向量集合,同时将不变的操作数存入LoopConst（需要大量补充？？？？？？？？）
//循环不变运算可以是一元/二元/赋值/等等，这边具体参考我们已经有的所有可能的中间代码指令类型，需要仔细设计
std::vector<Instruction*> LoopOptimization::calculateLoopConstant(std::vector<BasicBlock*> Loop,Function* func){
    std::vector<Instruction*> LoopConstInstructions;
    std::set<Operand*>Operands; //避免重复
    //我们只能够保证，加入了loopConst的那些操作数，就代表了在循环中至少存在它的一条定值语句被标记为循环不变指令，但可能有多条！
    LoopConst[func][Loop]=Operands;
    //遍历当前循环的每一个基本块
    while(true){
        bool ifAddNew=false;
        for(auto block:Loop){
            Instruction* ins = block->begin();
            Instruction* last = block->end();
            //遍历这个基本块的所有指令，while循环中ins每次在最后面“+1”
            while (ins != last) {
                //如果当前的指令它没有存入LoopConstInstructions，我们才去讨论它，否则略去
                if(!count(LoopConstInstructions.begin(),LoopConstInstructions.end(),ins)){
                    //如果指令类型是二元
                    if(ins->isBinary()){
                        //获取当前指令的use
                        std::vector<Operand*> useOperands=ins->getUse();
                        int constant_count=0;
                        
                        //还需要考虑二元操作数中类型是ArrayType或者PointerType!!!!!!!!!!!!!!!!!!!
                        //以及a=func(1)+3，这里面func(1)的类型为int or float，当然这并不重要，只要我们能够在call语句上确定func(1)是循环不变量，就可以

                        //遍历所有的use操作数
                        for(auto operand:useOperands){
                            //获取它们的类型
                            Type * operand_type=operand->getEntry()->getType();
                            //如果是int
                            if(operand_type->isInt()){
                                IntType* operand_inttype=(IntType*)operand_type;
                                //是常量
                                if(operand_inttype->isConst()){
                                    //插入LoopConst
                                    LoopConst[func][Loop].insert(operand);
                                    constant_count++;
                                }
                                //对该操作数的定值运算全部被标记为循环不变
                                else if(OperandIsLoopConst(operand,Loop,LoopConstInstructions)){
                                    LoopConst[func][Loop].insert(operand);
                                    constant_count++;
                                }
                            }
                            //如果是float
                            else if(operand_type->isFloat()){
                                FloatType* operand_floattype=(FloatType*)operand_type;
                                if(operand_floattype->isConst()){
                                    LoopConst[func][Loop].insert(operand);
                                    constant_count++;
                                }
                                //对该操作数的定值运算全部被标记为循环不变
                                else if(OperandIsLoopConst(operand,Loop,LoopConstInstructions)){
                                    LoopConst[func][Loop].insert(operand);
                                    constant_count++;
                                }
                            }
                        }
                        //二元运算，需要两个use操作数都是循环不变量，那么这条指令就是循环不变运算
                        if(constant_count==2){
                            // ins->output();
                            LoopConstInstructions.push_back(ins);
                            ifAddNew=true;
                        }
                    }
                    // 如果指令类型为alloc,无条件接受，因为他只可能是循环中的数组定义
                    else if(ins->isAlloc()){
                        LoopConstInstructions.push_back(ins);
                        LoopConst[func][Loop].insert(ins->getDef());
                        ifAddNew=true;
                    }
                    //wait for adding
                }
                ins=ins->getNext();
            }
        }
        if(!ifAddNew) break;
    }
    return LoopConstInstructions;
}

// //检测当前的操作数op的定值是否有在循环内存在，若有，则返回true
// bool LoopOptimization::defInstructionInLoop(Operand * op,std::vector<BasicBlock*>Loop){
//     for(auto block:Loop){
//         Instruction* i = block->begin();
//         Instruction* last = block->end();
//         while (i != last) {
//             if(i->getDef()==op){
//                 return true;
//             }
//             i = i->getNext();
//         }
//     }
//     return false;
// }

//但凡在循环中存在一条对该操作数的定值语句，它没有被标记为循环不变语句，就不通过
//若定值全在外，或有在里面，但都被标记，则true
//现在全部的依凭就是LoopConstInstructions
bool LoopOptimization::OperandIsLoopConst(Operand * op,std::vector<BasicBlock*>Loop,std::vector<Instruction*> LoopConstInstructions){
    for(auto block:Loop){
        Instruction* i = block->begin();
        Instruction* last = block->end();
        while (i != last) {
            //在循环中找到了一条定值语句,但是遇到数组是有bug的(见文档)，这个再讨论
            if(i->getDef()==op){
                //如果这条定值语句目前不是循环不变语句，那就不通过
                if(!count(LoopConstInstructions.begin(),LoopConstInstructions.end(),i)){
                    return false;
                }
            }
            i = i->getNext();
        }
    }
    return true;

    /*
    //只有一个定值能到达,做了ssa，则只有一个def
    for(auto ins:LoopConstInstructions){
        if(ins->isStore())
            continue;
        if(ins->getDef()==op){
            return true;
        }
    }
    return false;
    */
}

/*
//phi指令的块不在循环里，改成对应的preblock
void LoopOptimization::changePhiInstruction(std::vector<BasicBlock*>& Loop,BasicBlock* preBlock, std::vector<BasicBlock*> oldBlocks){
    for(auto block:Loop){
        Instruction* i = block->begin();
        Instruction* last = block->end();
        while (i != last) {
            //是phi指令
            if(i->isPhi()){
                PhiInstruction* pi=(PhiInstruction*)i;
                //判断phi指令的src块是否在循环里

                for(auto oldBlock:oldBlocks){
                    
                    if(pi->findSrc(oldBlock)){
                        Operand* op=pi->getSrc(oldBlock);
                        pi->removeSrc(oldBlock);
                        pi->addSrc(preBlock,op);
                    }
                }
            }
            i = i->getNext();
        }
    }
}
*/

#include "LoopUnroll.h"
using namespace std;

void LoopUnroll::calculateCandidateLoop(vector<vector<BasicBlock*>> LoopList){
    // 理应只针对最内循环做展开
    vector<vector<BasicBlock*>> InnerLoop;
    for(auto loopi:LoopList){
        bool inner =true;
        for(auto loopj:LoopList){
            if(isSubset(loopj,loopi)){
                inner=false;
                break;
            }
        }
        if(inner){
            InnerLoop.push_back(loopi);
            //cout<<loopi[0]->getNo()<<endl;
        }
    }

    //我们仅处理内部循环中仅一个基本块的循环,封装后均压入candidateLoops
    //暂时不考虑多个基本块的循环（比如循环内还有条件判断等）
    for(auto loop1:InnerLoop){
        if(loop1.size()!=1){
            continue;
        }
        //下面的是InnerLoop.size==1()的，也就是循环就一个基本块
        BasicBlock* cond=nullptr;
        BasicBlock* body=loop1[0];
        //前驱只能有俩，因为是自循环，所以一个前驱必定是自己；另一个就是外部的一个基本块
        //这个前驱的情况比较复杂，至少有3种，甚至可能没有
        //assert(body->getPred().size()==2);
        for(auto bb:body->getPred()){
            if(bb!=body){
                cond=bb;
                break;
            }
        }

        loop* l = new loop();
        l->setbody(body);
        l->setcond(cond); //可能为空
        candidateLoops.push_back(l);
        //cout<<l->getbody()->getNo()<<endl;
    }
}

bool LoopUnroll::isSubset(vector<BasicBlock*> son, vector<BasicBlock*> farther){
    for(auto sonBB:son){
        if(!count(farther.begin(),farther.end(),sonBB)){
            return false;
        }
    }
    //避免son和farther是同一个循环
    return son.size()!=farther.size();
}

void LoopUnroll::Unroll(){
    for(auto loop:candidateLoops){
        //包含call指令的不展开(call系统函数的或许可以展开？)
        bool hasCall = false;
        BasicBlock* cond = loop->getcond(), *body = loop->getbody();
        for(auto bodyinstr = body->begin(); bodyinstr != body->end(); bodyinstr = bodyinstr->getNext()){
            if(bodyinstr->isCall()){
                IdentifierSymbolEntry* funcSE = (IdentifierSymbolEntry*)(((CallInstruction*)bodyinstr)->getFunc());
                if (!funcSE->isSysy() && funcSE->getName() != "llvm.memset.p0i8.i32") {
                    hasCall = true;
                    break;
                }
            }
        }
        if(hasCall){
            continue;
        }
        
        /*
        考虑最简单的循环，只有一个归纳变量i，在循环之外会有一个初始值begin，循环中会不断增加/减少值
        设这个变化的值为步长step，我们也只考虑最简单的，i在循环中只有一条二元指令表示其变化，如i=i+1；
        临界条件就是i和某个值做大小判断，设临界值为end；我们将i表示为strideOp，初始赋值的变量为beginOp，临界值为endOp，
        */

        //在body的cmp语句中，我们想要找到strideOp和endOp，这两个运算符的情况可能相当复杂,endOp也可是变化的，并求出beginOp
        //我们不处理大小判断的运算符为eq或not eq,因为wile的条件判定一般没有这两个
        int begin = -1, end = -1, step = -1;
        bool isBeginCons, isEndCons, isStepCons;
        isBeginCons = isEndCons = isStepCons = false;
        Operand* endOp,*beginOp=nullptr,*strideOp;
        bool isIncrease=true; //循环变量strideOp一开始较小，循环中不断变大

        stack<Instruction*> InsStack; //存储strideOp的指令def序列
        stack<Instruction*> InsStack1;//仅用于测验endOp是否随循环变化
        stack<Instruction*> temp;//仅用于清空
        Instruction* bodyCmp = nullptr;
        bool endOpChangeWithCycle=true;
        for(auto bodyinstr = body->begin(); bodyinstr != body->end(); bodyinstr=bodyinstr->getNext()){
            if(bodyinstr->isCmp()){
                bodyCmp = bodyinstr;
                Operand* cmpOp1=bodyCmp->getUse()[0];
                Operand* cmpOp2=bodyCmp->getUse()[1];
                int opcode=bodyCmp->getOpCode();
                //我们能够保证的是，如果其中一个为常量，那么它一定是endOp
                //如果两个都不是常量，我们暂定较小的那个为strideOp
                //如果根据这个op无法找到beginOp->它没有phi的Def，它的def可能追溯到全局or参数，就找另一个
                //有可能i<j，然后两个op都可以追述到phi指令（一个变大；一个变小），这种情况我们是无法处理的，要排除
                //也就是endOp不能随着循环变化而有变化！
                switch (opcode)
                {
                    case CmpInstruction::G:
                    case CmpInstruction::GE:
                        endOp=cmpOp1;
                        strideOp=cmpOp2;
                        if(strideOp->getEntry()->isConstant()){
                            strideOp=cmpOp1;
                            endOp=cmpOp2;
                            isIncrease=false;
                            endOpChangeWithCycle=false;
                        }
                        beginOp = getBeginOp(body,strideOp,InsStack);
                        //较小的那个Op回朔尝试失败，也就是这个op作为endop，不可能随循环变化
                        if(!(endOp->getEntry()->isConstant())){
                            if(beginOp==nullptr){ 
                                InsStack.swap(temp);
                                endOp=cmpOp2;
                                strideOp=cmpOp1;
                                beginOp = getBeginOp(body,strideOp,InsStack);
                                isIncrease=false;                                          
                                endOpChangeWithCycle=false;
                            }
                            else if(getBeginOp(body,endOp,InsStack1)==nullptr){
                                //追朔另一个op，看是不是随循环变化,这里不能用InsStack，否则后面判断会出错
                                endOpChangeWithCycle=false;                         
                            }                            
                        }
                        else{
                            endOpChangeWithCycle=false;
                        }
                        break;
                    case CmpInstruction::L:
                    case CmpInstruction::LE:
                        endOp=cmpOp2;
                        strideOp=cmpOp1;
                        if(strideOp->getEntry()->isConstant()){
                            strideOp=cmpOp2;
                            endOp=cmpOp1;
                            isIncrease=false;
                            endOpChangeWithCycle=false;
                        }
                        beginOp=getBeginOp(body,strideOp,InsStack);
                        //较小的那个Op回朔尝试失败
                        if(!(endOp->getEntry()->isConstant())){
                            if(beginOp==nullptr){ 
                                InsStack.swap(temp);
                                endOp=cmpOp1;
                                strideOp=cmpOp2;
                                beginOp = getBeginOp(body,strideOp,InsStack);
                                isIncrease=false;                                
                                endOpChangeWithCycle=false;
                            }
                            else if(getBeginOp(body,endOp,InsStack1)==nullptr){
                                //追朔另一个op，看是不是随循环变化,这里不能用InsStack，否则后面判断会出错
                                endOpChangeWithCycle=false;                         
                            }                            
                        }
                        else{
                            endOpChangeWithCycle=false;
                        }
                        break;
                    default:
                        //E或NE的情况不做处理
                        //cout<<"bodycmp is ne or e"<<endl;
                        break;
                }
            }
        }
        if(beginOp==nullptr){
            //cout<<"begin op is null"<<endl;
            continue;
        }
        if(endOpChangeWithCycle){
            //cout<<"endOpChangeWithCycle"<<endl;
            continue;
        }

        //打印三个op及变化关系
        // cout<<"beginOp: "<<beginOp->toStr()<<endl;
        // cout<<"strideOp: "<<strideOp->toStr()<<" "<<isIncrease<<endl;
        // cout<<"endOp: "<<endOp->toStr()<<endl;

        //我们暂时先只考虑归纳变量仅变化一次的情况，也就是只有形如i=i+1这种
        int ivOpcode=-1;
        Operand* stepOp=nullptr; //表示步长的操作数
        if(InsStack.size()!=2){
            //cout<<"InsStack.size()!=2"<<endl;
            continue;
        }
        Instruction* topIns=InsStack.top();
        if(topIns->isPhi()){
            InsStack.pop();
            Instruction *ins=InsStack.top();
            if(ins->isBinary()){
                ivOpcode=ins->getOpCode();
                for(auto useOp:ins->getUse()){
                    //这里的isParam也小心存在问题，但大体应该是进不来
                    if(useOp->getEntry()->isConstant()||useOp->isParam()){
                        stepOp = useOp;
                    }
                    //step在循环外定值
                    else if(useOp->getDef()->getParent()!=body){
                        stepOp = useOp;
                    }
                }
            }
            else{
                //cout<<"the iv ins not bin"<<endl;
                continue;
            }
        }
        else{
            //cout<<"the top ins in stack is not phi"<<endl;
            continue;
        }
        if(stepOp==nullptr){
            continue;
        }

        //打印stepOp
        //cout<<"stepOp: "<<stepOp->toStr()<<endl;

        //若是常量，存取其值
        if(beginOp->getEntry()->isConstant()){
            isBeginCons=true;
            begin=((ConstantSymbolEntry*)(beginOp->getEntry()))->getValue();
        }
        if(endOp->getEntry()->isConstant()){
            isEndCons=true;
            end=((ConstantSymbolEntry*)(endOp->getEntry()))->getValue();
        }
        if(stepOp->getEntry()->isConstant()){
            isStepCons=true;
            step=((ConstantSymbolEntry*)(stepOp->getEntry()))->getValue();
        }

        //我们这边只考虑说步长step为常量的，比较好判断
        if(isStepCons){
            //能够清晰的计算出先前总共要进行的循环轮数
            if(isBeginCons&&isEndCons){
                //先讨论说，变量strideOp的值随循环是不断增大的,begin<end
                int count=0;
                if(isIncrease){
                    if(ivOpcode==BinaryInstruction::ADD){
                        //有等号没等号，count是有差别的
                        if(bodyCmp->getOpCode()==CmpInstruction::GE||bodyCmp->getOpCode()==CmpInstruction::LE){
                            for(int i=begin;i<=end;i=i+step){
                                count++;
                            }
                        }
                        else if(bodyCmp->getOpCode()==CmpInstruction::G||bodyCmp->getOpCode()==CmpInstruction::L){
                            for(int i=begin;i<end;i=i+step){
                                count++;
                            }
                        }
                    }
                    else if(ivOpcode==BinaryInstruction::SUB){
                        //有等号没等号，count是有差别的
                        if(bodyCmp->getOpCode()==CmpInstruction::GE||bodyCmp->getOpCode()==CmpInstruction::LE){
                            for(int i=begin;i<=end;i=i-step){
                                count++;
                            }
                        }
                        else if(bodyCmp->getOpCode()==CmpInstruction::G||bodyCmp->getOpCode()==CmpInstruction::L){
                            for(int i=begin;i<end;i=i-step){
                                count++;
                            }
                        }                
                    }
                    else if(ivOpcode==BinaryInstruction::MUL){
                        //有等号没等号，count是有差别的
                        if(bodyCmp->getOpCode()==CmpInstruction::GE||bodyCmp->getOpCode()==CmpInstruction::LE){
                            for(int i=begin;i<=end;i=i*step){
                                count++;
                            }
                        }
                        else if(bodyCmp->getOpCode()==CmpInstruction::G||bodyCmp->getOpCode()==CmpInstruction::L){
                            for(int i=begin;i<end;i=i*step){
                                count++;
                            }
                        }
                    }
                    else if(ivOpcode==BinaryInstruction::DIV){
                        //有等号没等号，count是有差别的
                        if(bodyCmp->getOpCode()==CmpInstruction::GE||bodyCmp->getOpCode()==CmpInstruction::LE){
                            for(int i=begin;i<=end;i=i/step){
                                count++;
                            }
                        }
                        else if(bodyCmp->getOpCode()==CmpInstruction::G||bodyCmp->getOpCode()==CmpInstruction::L){
                            for(int i=begin;i<end;i=i/step){
                                count++;
                            }
                        }
                    }
                    else {
                        //cout<<"stride calculate not add sub mul div"<<endl;
                        continue;
                    }
                }
                else{
                    //讨论变量strideOp的值随循环是不断变小的,end<begin
                    //取模运算，应该是变小的，没有模一个负数的说法
                    if(ivOpcode==BinaryInstruction::ADD){
                        //有等号没等号，count是有差别的
                        if(bodyCmp->getOpCode()==CmpInstruction::GE||bodyCmp->getOpCode()==CmpInstruction::LE){
                            for(int i=begin;i>=end;i=i+step){
                                count++;
                            }
                        }
                        else if(bodyCmp->getOpCode()==CmpInstruction::G||bodyCmp->getOpCode()==CmpInstruction::L){
                            for(int i=begin;i>end;i=i+step){
                                count++;
                            }
                        }
                    }
                    else if(ivOpcode==BinaryInstruction::SUB){
                        //有等号没等号，count是有差别的
                        if(bodyCmp->getOpCode()==CmpInstruction::GE||bodyCmp->getOpCode()==CmpInstruction::LE){
                            for(int i=begin;i>=end;i=i-step){
                                count++;
                            }
                        }
                        else if(bodyCmp->getOpCode()==CmpInstruction::G||bodyCmp->getOpCode()==CmpInstruction::L){
                            for(int i=begin;i>end;i=i-step){
                                count++;
                            }
                        }                
                    }
                    else if(ivOpcode==BinaryInstruction::MUL){
                        //有等号没等号，count是有差别的
                        if(bodyCmp->getOpCode()==CmpInstruction::GE||bodyCmp->getOpCode()==CmpInstruction::LE){
                            for(int i=begin;i>=end;i=i*step){
                                count++;
                            }
                        }
                        else if(bodyCmp->getOpCode()==CmpInstruction::G||bodyCmp->getOpCode()==CmpInstruction::L){
                            for(int i=begin;i>end;i=i*step){
                                count++;
                            }
                        }
                    }
                    else if(ivOpcode==BinaryInstruction::DIV){
                        //有等号没等号，count是有差别的
                        if(bodyCmp->getOpCode()==CmpInstruction::GE||bodyCmp->getOpCode()==CmpInstruction::LE){
                            for(int i=begin;i>=end;i=i/step){
                                count++;
                            }
                        }
                        else if(bodyCmp->getOpCode()==CmpInstruction::G||bodyCmp->getOpCode()==CmpInstruction::L){
                            for(int i=begin;i>end;i=i/step){
                                count++;
                            }
                        }
                    }
                    else {
                        //cout<<"stride calculate not add sub mul div"<<endl;
                        continue;
                    }
                }
                //指令copy count 份
                //body中的跳转指令不copy
                //循环内部是小于count 所以count初始值直接设置为0即可
                /*
                * Special:  pred -> body -> Exitbb      ==>     pred -> newbody -> Exitbb
                */
                if(count>0&&count<=MAXUNROLLNUM){
                    //cout<<"specialUnroll: count = "<<count<<endl;
                    specialUnroll(body,count,endOp,strideOp,true);
                }
                //如果count超过了max，就特殊展开，这边先不做
            }
            else{
                //begin和end有其中一个不是const类型,就是normal展开
                //展开四次,copy四次,构建rescond resbody
                //rescond包含 最后算出来的变量值 然后新建一条cmp指令 最后算出来的变量值与end作比较，重构一个循环即可
                //看n是否为temp,后续的stride继续补充即可
                //面向样例编程，只考虑形如i=i+1;i=i-1的展开
                /*
                *                     --------                                    ------------        -----------
                *                     ↓      ↑                                    ↓          ↑        ↓         ↑
                * Normal:  pred ->  cond -> body  Exitbb      ==>     pred -> rescond -> resbody maincond -> mainbody Exitbb
                *                     ↓             ↑                             ↓                  ↑   ↓              ↑
                *                     ---------------                             --------------------   ----------------                
                */
                if(ivOpcode==BinaryInstruction::ADD&&step==1){
                    //cout<<"normalUnroll"<<endl;
                    // normalUnroll(cond,body,beginOp,endOp,strideOp);             
                }
                if(ivOpcode==BinaryInstruction::SUB&&step==1){

                }
            }
        }
    }
}

//找循环中不断变化的变元strideOp的初始值beginOp
Operand* LoopUnroll::getBeginOp(BasicBlock* bb,Operand* strideOp,stack<Instruction*>& InsStack){
    Operand* temp=strideOp;
    if(temp->getDef()->getParent()!=bb){
        return nullptr;
    }
    while(!temp->getDef()->isPhi()){
        Instruction* tempdefIns=temp->getDef();
        InsStack.push(tempdefIns);
        vector<Operand*> uses=tempdefIns->getUse();
        bool iftempChange=false;

        //像全局关联的变量，不断向上追溯就是一条load语句，我们也暂时默认他不能作为strideOp
        if(uses.size()!=2){
            //cout<<"uses.size()!=2"<<endl;
            return nullptr;
        }
        Operand* useOp1=uses[0],*useOp2=uses[1];
        if(isRegionConst(useOp1,useOp2)){
            temp=useOp1;
            iftempChange=true;
        }
        else if(isRegionConst(useOp2,useOp1)){
            temp=useOp2;
            iftempChange=true;
        }
        if(!iftempChange||(temp->getDef()->getParent()!=bb)){
            //cout<<"temp no change or temp def bb not right"<<endl;
            return nullptr;
        }
    }

    //找到位于当前基本块bb中根源的phi指令
    PhiInstruction* phi=(PhiInstruction*)temp->getDef();
    InsStack.push(temp->getDef());
    Operand* beginOp;
    for(auto item:phi->getSrcs()){
        if(item.first!=bb){
            beginOp = item.second;
        }
    }
    return beginOp;
}

bool LoopUnroll::isRegionConst(Operand* i, Operand* c){
    //常数，总感觉下面这些判断过于粗糙
    if(c->getEntry()->isConstant()){
        return true;
    }
    else if(c->isGlobal()){
        return false;
    }
    else if(c->isParam()){
        return true;
    }
    //c BB dom i BB
    else if(c->getDef()&&i->getDef()){
        BasicBlock* c_farther=c->getDef()->getParent();
        BasicBlock* i_farther=i->getDef()->getParent();
        vector<BasicBlock*> Dom_i_Farther=DomBBSet[i_farther->getParent()][i_farther];
        if(count(Dom_i_Farther.begin(),Dom_i_Farther.end(),c_farther)){
            return true;
        }
    }
    return false;
}

void LoopUnroll::specialUnroll(BasicBlock* bb,int num,Operand* endOp,Operand* strideOp,bool ifall){
    vector<Instruction*> preInsList;
    vector<Instruction*> nextInsList;
    vector<Instruction*> phis;
    vector<Instruction*> copyPhis;
    CmpInstruction* cmp;
    vector<Operand*> finalOperands;
    map<Operand*,Operand*> begin_final_Map;

    //拷贝phi，存储cmp前面的指令到preInsList
    for(auto bodyinstr = bb->begin(); bodyinstr != bb->end(); bodyinstr = bodyinstr->getNext()){
        if(bodyinstr->isPhi()){
            phis.push_back(bodyinstr);
        }
        else if(bodyinstr->isCmp()){
            cmp=(CmpInstruction*) bodyinstr;
            break;
        }
        preInsList.push_back(bodyinstr);
    }
    copyPhis.assign(phis.begin(),phis.end());

    //拷贝pre指令放到下一轮，生成并更换原先pre指令中的Def和相关Use，预先存储展开前的那些def
    for(auto preIns:preInsList){
        Instruction* ins=preIns->copy();
        //store和一些call没有def
        if(preIns->getDef()){
            Operand* newDef = new Operand(new TemporarySymbolEntry(preIns->getDef()->getType(),SymbolTable::getLabel()));
            begin_final_Map[preIns->getDef()]=newDef;
            finalOperands.push_back(preIns->getDef());
            ins->setDef(newDef);
            preIns->replaceDef(newDef);
        }
        nextInsList.push_back(ins);
    }
    for(auto preIns:preInsList){
        for(auto useOp:preIns->getUse()){
            if(begin_final_Map.find(useOp)!=begin_final_Map.end()){
                preIns->replaceUse(useOp,begin_final_Map[useOp]);
            }
        }
    }
    for(auto nextIns:nextInsList){
        for(auto useOp:nextIns->getUse()){
            if(begin_final_Map.find(useOp)!=begin_final_Map.end()){
                nextIns->replaceUse(useOp,begin_final_Map[useOp]);
            }
        }
    }

    //进行循环展开
    std::map<Operand*, Operand*> replaceMap;
    for(int k = 0 ;k < num-1; k++){
        std::vector<Operand*> notReplaceOp;
        int calculatePhi = 0;
        for(int i=0;i<nextInsList.size();i++){
            Instruction* preIns=preInsList[i];
            Instruction* nextIns=nextInsList[i];
            
            if(preIns->getDef()){
                Operand* newDef = new Operand(new TemporarySymbolEntry(preIns->getDef()->getType(),SymbolTable::getLabel()));
                replaceMap[preIns->getDef()]=newDef;
                if(count(copyPhis.begin(),copyPhis.end(),preIns)){
                    PhiInstruction* phi = (PhiInstruction*)phis[calculatePhi];
                    nextInsList[i] =(Instruction*)(new BinaryInstruction(BinaryInstruction::ADD,newDef,phi->getBlockSrc(bb),new Operand(new ConstantSymbolEntry(preIns->getDef()->getType(),0)),nullptr));
                    notReplaceOp.push_back(newDef);
                    calculatePhi++;
                    copyPhis.push_back(nextInsList[i]);
                }
                else{
                    nextIns->setDef(newDef);
                }
            }
        }

        for(auto nextIns:nextInsList){
            if(nextIns->getDef()&&count(notReplaceOp.begin(),notReplaceOp.end(),nextIns->getDef())){
                continue;
            }
            for(auto useOp:nextIns->getUse()){
                if(replaceMap.find(useOp)!=replaceMap.end()){
                    nextIns->replaceUse(useOp,replaceMap[useOp]);
                }
                else{
                    useOp->addUse(nextIns);
                }
            }
        }

        for(auto ins:phis){
            PhiInstruction* phi=(PhiInstruction*) ins;
            Operand* old=phi->getBlockSrc(bb);
            Operand* _new=replaceMap[old];
            phi->replaceUse(old,_new);
        }

        //最后一次才会换 否则不换
        if(k==num-2){
            // 构建新的map然后再次替换
            std::map<Operand*,Operand*> newMap;
            int i=0;
            for(auto nextIns:nextInsList){
                if(nextIns->getDef()){
                    newMap[nextIns->getDef()]=finalOperands[i];
                    nextIns->replaceDef(finalOperands[i]);
                    i++;
                }
            }
            for(auto nextIns:nextInsList){
                for(auto useOp:nextIns->getUse()){
                    if(newMap.find(useOp)!=newMap.end()){
                        nextIns->replaceUse(useOp,newMap[useOp]);
                    }
                }
                bb->insertBefore(nextIns,cmp);
            }
            for(auto ins:phis){
                PhiInstruction* phi=(PhiInstruction*) ins;
                Operand* old=phi->getBlockSrc(bb);
                Operand* _new=newMap[old];
                phi->replaceUse(old,_new);
            }
        }
        else{
            for(auto nextIns:nextInsList){
                bb->insertBefore(nextIns,cmp);
            }
        }

        //清空原来的
        preInsList.clear();
        //复制新的到pre
        preInsList.assign(nextInsList.begin(),nextInsList.end());
        //清空next
        nextInsList.clear();
        for(auto preIns:preInsList){
            nextInsList.push_back(preIns->copy());
        }
    }

    //如果是完全张开的话
    if(ifall){
        //去掉phi指令，new一条二元的add 0指令存储循环变元i在循环外的初始值
        for(auto phi:phis){
            PhiInstruction* p = (PhiInstruction*) phi;
            Operand* phiOp;
            for(auto item:p->getSrcs()){
                if(item.first!=bb){
                    phiOp = item.second;
                }
            }
            BinaryInstruction * newDefBin = new BinaryInstruction(BinaryInstruction::ADD,phi->getDef(),phiOp,new Operand(new ConstantSymbolEntry(phiOp->getEntry()->getType(),0)),nullptr);
            Instruction* phiNext = phi->getNext();
            bb->remove(phi);
            bb->insertBefore(newDefBin,phiNext);
        }

        //去除块中的比较跳转指令，完全展开后直接跳转到exit基本块即可
        CondBrInstruction* cond = (CondBrInstruction*)cmp->getNext();
        UncondBrInstruction* newUnCond = new UncondBrInstruction(cond->getFalseBranch(),nullptr); 
        bb->remove(cmp);
        bb->remove(cond);
        bb->insertBack(newUnCond);
        bb->removePred(bb);
        bb->removeSucc(bb);
    }    
}

void LoopUnroll::normalUnroll(BasicBlock* condbb,BasicBlock* bodybb,Operand* beginOp,Operand* endOp,Operand* strideOp){
    //如果说begin或end不全为const类型的话，就保证了它前面一定有一个cond基本块，含cmp指令, 但可能有外提    
    //先不处理外提
    Instruction* condCmp=nullptr;
    for(auto ins=condbb->begin();ins!=condbb->end();ins=ins->getNext()){
        if(ins->isCmp()){
            if(!(ins->getOpCode()==CmpInstruction::E||ins->getOpCode()==CmpInstruction::NE)){
                condCmp=ins;
                break;
            }
        }
    }
    if(condCmp==nullptr){
        return;
    }

    BasicBlock* newCondBB = new BasicBlock(condbb->getParent());
    BasicBlock* resBodyBB = new BasicBlock(condbb->getParent());
    BasicBlock* resOutCond = new BasicBlock(condbb->getParent());
    BasicBlock* resoutCondSucc=nullptr;
    for(auto succBB:bodybb->getSucc()){
        if(succBB!=bodybb)
            resoutCondSucc=succBB;
    }
    if(resoutCondSucc==nullptr){
        return;
    }

    std::vector<Instruction*> InstList;
    CmpInstruction* cmp=nullptr;
    for(auto bodyinstr = bodybb->begin(); bodyinstr != bodybb->end(); bodyinstr = bodyinstr->getNext()){
        if(bodyinstr->isCmp()){
            cmp=(CmpInstruction*) bodyinstr;
            break;
        }  
        InstList.push_back(bodyinstr);
    }
    
    //不像special的情况，我们无法预先计算出循环次数，但可以通过插入指令，在arm代码中动态计算
    bool ifPlusOne = false;
    BinaryInstruction* binPlusOne;
    //变元i每次加1，如果cmp判定条件有等于的话，循环次数要加上一次
    if(cmp->getOpCode()==CmpInstruction::LE||cmp->getOpCode()==CmpInstruction::GE){
        ifPlusOne = true;
    }
    Operand* countDef = new Operand(new TemporarySymbolEntry(TypeSystem::intType,SymbolTable::getLabel()));
    //使用endOp-beginOp计算循环次数，存入calCount（为中间变量）
    BinaryInstruction* calCount = new BinaryInstruction(BinaryInstruction::SUB,countDef,endOp,beginOp,nullptr);
    //如果循环次数需要加1，就new一条add 1的指令
    if(ifPlusOne){
        binPlusOne = new BinaryInstruction(BinaryInstruction::ADD,new Operand(new TemporarySymbolEntry(TypeSystem::intType,SymbolTable::getLabel())),countDef,new Operand(new ConstantSymbolEntry(TypeSystem::intType,1)),nullptr);
        countDef = binPlusOne->getDef();
    }
    //假设要展开4次，计算循环次数countDef%4是否为0
    //如果为0，我们就可以把bodybb按4次进行展开，跳到bodybb，否则resBodyBB
    BinaryInstruction* binMod = new BinaryInstruction(BinaryInstruction::MOD,new Operand(new TemporarySymbolEntry(TypeSystem::intType,SymbolTable::getLabel())),countDef,new Operand(new ConstantSymbolEntry(TypeSystem::intType,UNROLLNUM)),nullptr);
    CmpInstruction* cmpEZero = new CmpInstruction(CmpInstruction::E,new Operand(new TemporarySymbolEntry(TypeSystem::boolType,SymbolTable::getLabel())),binMod->getDef(),new Operand(new ConstantSymbolEntry(TypeSystem::intType,0)),nullptr);
    CondBrInstruction* condBr = new CondBrInstruction(bodybb,resBodyBB,cmpEZero->getDef(),nullptr); 

    //newCondBB接在condbb后面，用以计算循环次数并判断是否模4为0
    newCondBB->addPred(condbb);
    newCondBB->addSucc(bodybb);
    newCondBB->addSucc(resBodyBB);
    newCondBB->insertBack(calCount);
    //得看是否有equal
    if(ifPlusOne){
        newCondBB->insertBack(binPlusOne);
    }
    newCondBB->insertBack(binMod);
    newCondBB->insertBack(cmpEZero);
    newCondBB->insertBack(condBr);
    Instruction* condBrIns=condCmp->getNext();
    ((CondBrInstruction*)condBrIns)->setTrueBranch(newCondBB);
    condbb->addSucc(newCondBB);
    condbb->removeSucc(bodybb);

    //resBody接在newCond后面，自成循环；循环出口为resOut
    resBodyBB->addPred(newCondBB);
    resBodyBB->addPred(resBodyBB);
    resBodyBB->addSucc(resBodyBB);
    resBodyBB->addSucc(resOutCond);
    //resBody第一条得是phi指令
    //末尾添加cmp和br指令
    std::vector<Instruction*> resBodyInstList;
    for(auto ins:InstList){
        resBodyInstList.push_back(ins->copy());
    }
    std::map<Operand*,Operand*> resBodyReplaceMap;
    for(auto resIns:resBodyInstList){
        if(resIns->getDef()){
            if(resIns->isPhi()){
                PhiInstruction* phi = (PhiInstruction*) resIns;
                Operand* condOp = phi->getBlockSrc(condbb);
                Operand* bodyOp = phi->getBlockSrc(bodybb);
                phi->removeBlockSrc(condbb);
                phi->removeBlockSrc(bodybb);
                phi->addSrc(newCondBB,condOp);
                phi->addSrc(resBodyBB,bodyOp);
            }
            Operand* oldDef = resIns->getDef(); 
            Operand* newDef = new Operand(new TemporarySymbolEntry(oldDef->getType(),SymbolTable::getLabel()));
            resBodyReplaceMap[oldDef]=newDef;
            resIns->setDef(newDef);
        }
    }

    Operand* resNumPhiDef = new Operand(new TemporarySymbolEntry(TypeSystem::intType,SymbolTable::getLabel()));
    Operand* binIncDef = new Operand(new TemporarySymbolEntry(TypeSystem::intType,SymbolTable::getLabel()));
    PhiInstruction* resNumPhi = new PhiInstruction(resNumPhiDef,nullptr);
    resNumPhi->addSrc(newCondBB,new Operand(new ConstantSymbolEntry(TypeSystem::intType,0)));
    resNumPhi->addSrc(resBodyBB,binIncDef);
    BinaryInstruction* binInc = new BinaryInstruction(BinaryInstruction::ADD,binIncDef,resNumPhi->getDef(),new Operand(new ConstantSymbolEntry(TypeSystem::intType,1)),nullptr);
    CmpInstruction* resBodyCmp = new CmpInstruction(CmpInstruction::NE,new Operand(new TemporarySymbolEntry(TypeSystem::boolType,SymbolTable::getLabel())),binIncDef,binMod->getDef(),nullptr);
    CondBrInstruction* resBodyBr = new CondBrInstruction(resBodyBB,resOutCond,resBodyCmp->getDef(),nullptr);

    for(auto resIns:resBodyInstList){
        for(auto useOp:resIns->getUse()){
            if(resBodyReplaceMap.find(useOp)!=resBodyReplaceMap.end()){
                resIns->replaceUse(useOp,resBodyReplaceMap[useOp]);
            }
            else{
                useOp->addUse(resIns);
            }
        }
    }
    
    resBodyBB->insertBack(resBodyBr);
    resBodyBB->insertBefore(resBodyCmp,resBodyBr);
    resBodyBB->insertBefore(binInc,resBodyCmp);
    resBodyBB->insertFront(resNumPhi,false);
    for(auto resIns:resBodyInstList){
        resBodyBB->insertBefore(resIns,binInc);
    }

    //resoutcond
    resOutCond->addPred(resBodyBB);
    resOutCond->addSucc(resoutCondSucc);
    resOutCond->addSucc(bodybb);
    resoutCondSucc->addPred(resOutCond);

    CmpInstruction* resOutCondCmp =(CmpInstruction*) cmp->copy();
    resOutCondCmp->setDef(new Operand(new TemporarySymbolEntry(TypeSystem::boolType,SymbolTable::getLabel())));
    resOutCondCmp->replaceUse(strideOp,resBodyReplaceMap[strideOp]);
    CondBrInstruction* resOutCondBr = new CondBrInstruction(bodybb,resoutCondSucc,resOutCondCmp->getDef(),nullptr);
    resOutCond->insertBack(resOutCondBr);
    resOutCond->insertBefore(resOutCondCmp,resOutCondBr);

    bodybb->removePred(condbb);
    bodybb->addPred(newCondBB);
    bodybb->addPred(resOutCond);

    for(int i=0;i<InstList.size();i++){
        if(InstList[i]->isPhi()){
            PhiInstruction* phi = (PhiInstruction*) InstList[i];
            Operand* condOp =  phi->getBlockSrc(condbb);
            Operand* bodyOp =  phi->getBlockSrc(bodybb); 
            phi->removeBlockSrc(condbb);
            phi->addSrc(newCondBB,condOp);
            phi->addSrc(resOutCond,resBodyReplaceMap[bodyOp]);
        }
    }

    specialUnroll(bodybb,UNROLLNUM,endOp,strideOp,false);

    //更改resoutSucc,加一个来自rescout的源
    for(auto bodyinstr = resoutCondSucc->begin(); bodyinstr != resoutCondSucc->end(); bodyinstr = bodyinstr->getNext()){
        if(bodyinstr->isPhi()){
            PhiInstruction* phi = (PhiInstruction*) bodyinstr;
            Operand* originalOperand = phi->getBlockSrc(bodybb);
            phi->addSrc(resOutCond,resBodyReplaceMap[originalOperand]);
        }
    }
}
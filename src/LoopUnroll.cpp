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
        //包含call指令的不展开
        bool hasCall = false;
        BasicBlock* cond = loop->getcond(), *body = loop->getbody();
        for(auto bodyinstr = body->begin(); bodyinstr != body->end(); bodyinstr = bodyinstr->getNext()){
            if(bodyinstr->isCall()){
                hasCall = true;
                break;
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
        Operand* endOp,*beginOp,*strideOp;
        bool isIncrease=true; //循环变量strideOp一开始较小，循环中不断变大

        stack<Instruction*> InsStack;
        Instruction* bodyCmp = nullptr;
        for(auto bodyinstr = body->begin(); bodyinstr != body->end(); bodyinstr=bodyinstr->getNext()){
            if(bodyinstr->isCmp()){
                bodyCmp = bodyinstr;
                Operand* cmpOp1=bodyCmp->getUse()[0];
                Operand* cmpOp2=bodyCmp->getUse()[1];
                int opcode=bodyCmp->getOpCode();
                //我们能够保证的是，如果其中一个为常量，那么它一定是endOp
                //如果两个都不是常量，我们暂定较小的那个为strideOp
                //如果根据这个op无法找到beginOp->它没有phi的Def，它的def可能追溯到全局or参数，就找另一个
                //其实也有可能i<j，然后两个op都可以追述到phi指令（一个变大；一个变小）
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
                        }
                        beginOp = getBeginOp(body,strideOp,InsStack);
                        //较小的那个Op回朔尝试失败
                        if(beginOp==nullptr&&!(endOp->getEntry()->isConstant())){
                            endOp=cmpOp2;
                            strideOp=cmpOp1;
                            beginOp = getBeginOp(body,strideOp,InsStack);
                            isIncrease=false;
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
                        }
                        beginOp=getBeginOp(body,strideOp,InsStack);
                        //较小的那个Op回朔尝试失败
                        if(beginOp==nullptr&&!(endOp->getEntry()->isConstant())){
                            endOp=cmpOp1;
                            strideOp=cmpOp2;
                            beginOp = getBeginOp(body,strideOp,InsStack);
                            isIncrease=false;
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

        //打印三个op及变化关系
        cout<<"beginOp: "<<beginOp->toStr()<<endl;
        cout<<"strideOp: "<<strideOp->toStr()<<" "<<isIncrease<<endl;
        cout<<"endOp: "<<endOp->toStr()<<endl;

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
        cout<<"stepOp: "<<stepOp->toStr()<<endl;

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
            
        }
    }
}

//找循环中不断变化的变元strideOp的初始值beginOp
Operand* LoopUnroll::getBeginOp(BasicBlock* bb,Operand* strideOp,stack<Instruction*>& InsStack){
    Operand* temp=strideOp;
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
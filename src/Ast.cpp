#include "Ast.h"
#include <stack>
#include <string>
#include <iostream>
#include <assert.h>
#include "IRBuilder.h"
#include "Instruction.h"
#include "SymbolTable.h"
#include "Type.h"
#include "Unit.h"

#define now_bb (builder->getInsertBB())
#define now_func (builder->getInsertBB()->getParent())
extern Unit unit;

extern FILE *yyout;
int Node::counter = 0;
IRBuilder *Node::builder = nullptr;

// 构造函数之类的代码区

Node::Node()
{
    seq = counter++;
    next = nullptr;
}

void Node::setNext(Node *node)
{
    Node *n = this;
    while (n->getNext())
    {
        n = n->getNext();
    }
    n->next = node;
}

BinaryExpr::BinaryExpr(SymbolEntry *se, int op, ExprNode *expr1, ExprNode *expr2) : ExprNode(se), op(op), expr1(expr1), expr2(expr2)
{
    this->dst = new Operand(se);
    this->type = se->getType();
    std::string op_type;
    switch (op)
    {
    case ADD:
        op_type = "+";
        break;
    case SUB:
        op_type = "-";
        break;
    case MUL:
        op_type = "*";
        break;
    case DIV:
        op_type = "/";
        break;
    case MOD:
        op_type = "%";
        break;
    case AND:
        op_type = "&&";
        break;
    case OR:
        op_type = "||";
        break;
    case LESS:
        op_type = "<";
        break;
    case LESSEQUAL:
        op_type = "<=";
        break;
    case GREATER:
        op_type = ">";
        break;
    case GREATEREQUAL:
        op_type = ">=";
        break;
    case EQUAL:
        op_type = "==";
        break;
    case NOTEQUAL:
        op_type = "!=";
        break;
    }
    if (expr1->getType()->isVoid() || expr2->getType()->isVoid())
    {
        fprintf(stderr, "invalid operand of type \'void\' to binary \'opeartor%s\'\n", op_type.c_str());
    }
    // 前几个操作符是算术运算符，返回类型是int型，后面是逻辑运算符，返回类型是Bool型
    Type *type1 = expr1->getType();
    Type *type2 = expr2->getType();
    bool hasFloat = (type1->isFloat() || type2->isFloat());
    if (op >= BinaryExpr::AND && op <= BinaryExpr::NOTEQUAL)
    {
        // 对于AND和OR逻辑运算，如果操作数表达式不是bool型，需要进行隐式转换，转为bool型。
        if (op == BinaryExpr::AND || op == BinaryExpr::OR)
        {
            if (type1->isInt())
                this->expr1 = new ImplictCastExpr(expr1, ImplictCastExpr::ITB);
            else if (type1->isFloat())
                this->expr1 = new ImplictCastExpr(expr1, ImplictCastExpr::FTB);
            if (type2->isInt())
                this->expr2 = new ImplictCastExpr(expr2, ImplictCastExpr::ITB);
            else if (type2->isFloat())
                this->expr2 = new ImplictCastExpr(expr2, ImplictCastExpr::FTB);
        }
        else
        {
            if (type1->isBool())
                this->expr1 = new ImplictCastExpr(expr1, hasFloat ? ImplictCastExpr::BTF : ImplictCastExpr::BTI);
            else if (type1->isInt() && hasFloat)
                this->expr1 = new ImplictCastExpr(expr1, ImplictCastExpr::ITF);
            if (type2->isBool())
                this->expr2 = new ImplictCastExpr(expr2, hasFloat ? ImplictCastExpr::BTF : ImplictCastExpr::BTI);
            else if (type2->isInt() && hasFloat)
                this->expr2 = new ImplictCastExpr(expr2, ImplictCastExpr::ITF);
        }
    }
    else if (op != BinaryExpr::MOD)
    { // 如果是MOD类型的话，两个操作数一定是int，不然它走不出语法分析
        // 这里不考虑bool类型做操作数的情况
        if (type1->isInt() && hasFloat)
            this->expr1 = new ImplictCastExpr(expr1, ImplictCastExpr::ITF);
        if (type2->isInt() && hasFloat)
        {
            this->expr2 = new ImplictCastExpr(expr2, ImplictCastExpr::ITF);
        }
    }
}

double BinaryExpr::getValue()
{
    // 这里虽然用double存储，但是如果结果是float，计算的时候要转成float，要不可能
    // 精度偏高感觉
    double value1 = expr1->getValue();
    double value2 = expr2->getValue();
    double value;
    if (type->isFloat())
    {
        float value1 = (float)expr1->getValue();
        float value2 = (float)expr2->getValue();
        float value;
        switch (op)
        {
        case ADD:
            value = value1 + value2;
            break;
        case SUB:
            value = value1 - value2;
            break;
        case MUL:
            value = value1 * value2;
            break;
        case DIV:
            value = value1 / value2;
            break;
        // case MOD: // 这里认为浮点数不能模
        //     value = value1 % value2;
        //     break;
        case AND:
            value = value1 && value2;
            break;
        case OR:
            value = value1 || value2;
            break;
        case LESS:
            value = value1 < value2;
            break;
        case LESSEQUAL:
            value = value1 <= value2;
            break;
        case GREATER:
            value = value1 > value2;
            break;
        case GREATEREQUAL:
            value = value1 >= value2;
            break;
        case EQUAL:
            value = value1 == value2;
            break;
        case NOTEQUAL:
            value = value1 != value2;
            break;
        }
        return value;
    }
    else // 如果最终结果为int，那么其实两个操作数类型都是int，没啥精度问题，直接算
    {
        switch (op)
        {
        case ADD:
            value = value1 + value2;
            break;
        case SUB:
            value = value1 - value2;
            break;
        case MUL:
            value = value1 * value2;
            break;
        case DIV:
            value = value1 / value2;
            break;
        case MOD:
            value = (int)value1 % (int)value2;
            break;
        case AND:
            value = value1 && value2;
            break;
        case OR:
            value = value1 || value2;
            break;
        case LESS:
            value = value1 < value2;
            break;
        case LESSEQUAL:
            value = value1 <= value2;
            break;
        case GREATER:
            value = value1 > value2;
            break;
        case GREATEREQUAL:
            value = value1 >= value2;
            break;
        case EQUAL:
            value = value1 == value2;
            break;
        case NOTEQUAL:
            value = value1 != value2;
            break;
        }
    }
    return value;
}

UnaryExpr::UnaryExpr(SymbolEntry *se, int op, ExprNode *expr) : ExprNode(se), op(op), expr(expr)
{
    this->dst = new Operand(se);
    this->type = se->getType();
    std::string op_str;
    switch (op)
    {
    case NOT:
        op_str = "!";
        break;
    case SUB:
        op_str = "-";
        break;
    }
    if (expr->getType()->isVoid())
    {
        fprintf(stderr, "invalid operand of type \'void\' to unary \'opeartor%s\'\n", op_str.c_str());
    }
    if (op == UnaryExpr::NOT)
    {
        // if (expr->getType()->isInt())
        // {
        //     this->expr = new ImplictCastExpr(expr);
        // }
    }
    else if (op == UnaryExpr::SUB)
    {
        if (expr->getType()->isBool())
        {
            this->expr = new ImplictCastExpr(expr, ImplictCastExpr::BTI);
        }
    }
}

double UnaryExpr::getValue()
{
    double value = expr->getValue();
    switch (op)
    {
    case NOT:
        value = (!value);
        break;
    case SUB:
        // 这里不用考虑精度，取负数就是符号位变一下
        value = (-value);
        break;
    }
    return value;
}

double Constant::getValue()
{
    return ((ConstantSymbolEntry *)symbolEntry)->getValue();
}

double Id::getValue()
{
    return ((IdentifierSymbolEntry *)symbolEntry)->getValue();
}

CallExpr::CallExpr(SymbolEntry *se, ExprNode *param) : ExprNode(se)
{
    dst = nullptr;
    // 统计实参数量
    unsigned long int paramCnt = 0;
    ExprNode *temp = param;
    while (temp)
    {
        params.push_back(temp);
        paramCnt++;
        temp = (ExprNode *)(temp->getNext());
    }
    // 由于存在函数重载的情况，这里我们提前将重载的函数通过符号表项的next指针连接，这里需要根据实参个数判断对应哪一个具体函数，找到其对应得符号表项
    SymbolEntry *s = se;
    Type *type;
    std::vector<Type *> FParams;
    while (s)
    {
        type = s->getType();
        FParams = ((FunctionType *)type)->getParamsType();
        if (paramCnt == FParams.size())
        {
            this->symbolEntry = s;
            break;
        }
        s = s->getNext();
    }
    if (symbolEntry)
    {
        // 如果函数返回值类型不为空，需要存储返回结果
        this->type = ((FunctionType *)type)->getRetType();
        if (this->type != TypeSystem::voidType)
        {
            SymbolEntry *se = new TemporarySymbolEntry(this->type, SymbolTable::getLabel());
            dst = new Operand(se);
        }
        ExprNode *temp = param;
        // 逐个比较形参列表和实参列表中每个参数的类型是否相同
        if (params.size() < FParams.size())
            fprintf(stderr, "too few arguments to function %s %s\n", symbolEntry->toStr().c_str(), type->toStr().c_str());
        else if (params.size() > FParams.size())
            fprintf(stderr, "too many arguments to function %s %s\n", symbolEntry->toStr().c_str(), type->toStr().c_str());
        auto it = FParams.begin();
        auto it2 = params.begin();
        for (; it != FParams.end(); it++, it2++)
        {
            if ((*it)->isFloat() && (*it2)->getType()->isInt())
                *it2 = new ImplictCastExpr(*it2, ImplictCastExpr::ITF);
            else if ((*it)->isInt() && (*it2)->getType()->isFloat())
                *it2 = new ImplictCastExpr(*it2, ImplictCastExpr::FTI);
            else if ((*it)->isInt() && (*it2)->getType()->isBool())
                *it2 = new ImplictCastExpr(*it2, ImplictCastExpr::BTI);
            else if ((*it)->isBool() && (*it2)->getType()->isInt())
                *it2 = new ImplictCastExpr(*it2, ImplictCastExpr::ITB);
            else if ((*it)->isFloat() && (*it2)->getType()->isBool())
                *it2 = new ImplictCastExpr(*it2, ImplictCastExpr::BTF);
            else if ((*it)->isBool() && (*it2)->getType()->isFloat())
                *it2 = new ImplictCastExpr(*it2, ImplictCastExpr::FTB);
            else if ((*it)->getKind() != (*it2)->getType()->getKind())
            {
                fprintf(stderr, "parameter's type %s can't convert to %s\n", (*it2)->getType()->toStr().c_str(), (*it)->toStr().c_str());
            }
        }
    }
    else
    {
        fprintf(stderr, "function is undefined\n");
    }
    if (((IdentifierSymbolEntry *)se)->isSysy())
    {
        unit.insertDeclare(se);
    }
}

IfStmt::IfStmt(ExprNode *cond, StmtNode *thenStmt) : cond(cond), thenStmt(thenStmt)
{
    if (cond->getType()->isInt())
        this->cond = new ImplictCastExpr(cond, ImplictCastExpr::ITB);
    else if (cond->getType()->isFloat())
        this->cond = new ImplictCastExpr(cond, ImplictCastExpr::FTB);
}

IfElseStmt::IfElseStmt(ExprNode *cond, StmtNode *thenStmt, StmtNode *elseStmt) : cond(cond), thenStmt(thenStmt), elseStmt(elseStmt)
{
    if (cond->getType()->isInt())
        this->cond = new ImplictCastExpr(cond, ImplictCastExpr::ITB);
    else if (cond->getType()->isFloat())
        this->cond = new ImplictCastExpr(cond, ImplictCastExpr::FTB);
}

WhileStmt::WhileStmt(ExprNode *cond, StmtNode *stmt) : cond(cond), stmt(stmt)
{
    if (cond->getType()->isInt())
        this->cond = new ImplictCastExpr(cond, ImplictCastExpr::ITB);
    else if (cond->getType()->isFloat())
        this->cond = new ImplictCastExpr(cond, ImplictCastExpr::FTB);
}

BreakStmt::BreakStmt(StmtNode *whileStmt) : whileStmt(whileStmt)
{
    if (whileStmt == nullptr)
    {
        fprintf(stderr, "no while stmt for this break stmt\n");
    }
}

ContinueStmt::ContinueStmt(StmtNode *whileStmt) : whileStmt(whileStmt)
{
    if (whileStmt == nullptr)
    {
        fprintf(stderr, "no while stmt for this continue stmt\n");
    }
}

ReturnStmt::ReturnStmt(ExprNode *retValue, Type *funcRetType) : retValue(retValue)
{
    // 判断返回值和函数返回值是否一致
    Type *retType;
    if (retValue == nullptr)
        retType = TypeSystem::voidType;
    else
        retType = retValue->getType();
    if (funcRetType->isFloat() && retType->isInt())
        this->retValue = new ImplictCastExpr(this->retValue, ImplictCastExpr::ITF);
    else if (funcRetType->isInt() && retType->isFloat())
        this->retValue = new ImplictCastExpr(this->retValue, ImplictCastExpr::FTI);
    else if (retType->getKind() != funcRetType->getKind())
        fprintf(stderr, "return type isn't equal to function type\n");
}

AssignStmt::AssignStmt(ExprNode *lval, ExprNode *expr) : lval(lval), expr(expr)
{
    Type *type = ((Id *)lval)->getType();
    Type *exprType = expr->getType();
    SymbolEntry *se = lval->getSymPtr();
    bool flag = true;
    if (type->isInt())
    {
        if (((IntType *)type)->isConst())
        {
            fprintf(stderr, "cannot assign to variable \'%s\' with const-qualified type \'%s\'\n",
                    ((IdentifierSymbolEntry *)se)->toStr().c_str(), type->toStr().c_str());
            flag = false;
        }
    }
    else if (type->isFloat())
    {
        if (((FloatType *)type)->isConst())
        {
            fprintf(stderr, "cannot assign to variable \'%s\' with const-qualified type \'%s\'\n",
                    ((IdentifierSymbolEntry *)se)->toStr().c_str(), type->toStr().c_str());
            flag = false;
        }
    }
    if (flag && expr->getType()->isBool())
    {
        fprintf(stderr, "cannot initialize a variable of type \'int\' with an rvalue of type \'%s\'\n",
                expr->getType()->toStr().c_str());
    }
    if (type->isInt() && exprType->isFloat())
    {
        this->expr = new ImplictCastExpr(this->expr, ImplictCastExpr::FTI);
    }
    else if (type->isFloat() && exprType->isInt())
    {
        this->expr = new ImplictCastExpr(this->expr, ImplictCastExpr::ITF);
    }
}

// -----------只因Code代码区------------------

void Node::backPatch(std::vector<Instruction *> &list, BasicBlock *bb)
{
    for (auto &inst : list)
    {
        if (inst->isCond())
            dynamic_cast<CondBrInstruction *>(inst)->setTrueBranch(bb);
        else if (inst->isUncond())
            dynamic_cast<UncondBrInstruction *>(inst)->setBranch(bb);
    }
}

std::vector<Instruction *> Node::merge(std::vector<Instruction *> &list1, std::vector<Instruction *> &list2)
{
    std::vector<Instruction *> res(list1);
    res.insert(res.end(), list2.begin(), list2.end());
    return res;
}

void Ast::genCode(Unit *unit)
{
    IRBuilder *builder = new IRBuilder(unit);
    Node::setIRBuilder(builder);
    root->genCode();
}

void BinaryExpr::genCode()
{
    if (op == AND)
    {
        if (this->isConde())
        {
            expr1->setIsCond(true);
            expr2->setIsCond(true);
        }
        BasicBlock *expr2BB = new BasicBlock(now_func); // if the result of lhs is true, jump to the trueBB.
        expr1->genCode();
        backPatch(expr1->trueList(), expr2BB);
        builder->setInsertBB(expr2BB); // set the insert point to the trueBB so that intructions generated by expr2 will be inserted into it.
        expr2->genCode();
        true_list = expr2->trueList();
        false_list = merge(expr1->falseList(), expr2->falseList());
    }
    else if (op == OR)
    {
        // Todo
        if (this->isConde())
        {
            expr1->setIsCond(true);
            expr2->setIsCond(true);
        }
        BasicBlock *expr2BB = new BasicBlock(now_func);
        expr1->genCode();
        backPatch(expr1->falseList(), expr2BB);
        builder->setInsertBB(expr2BB);
        expr2->genCode();
        true_list = merge(expr1->trueList(), expr2->trueList());
        false_list = expr2->falseList();
    }
    else if (op >= LESS && op <= NOTEQUAL)
    {
        // Todo
        expr1->genCode();
        expr2->genCode();
        int cmpOps[] = {CmpInstruction::L, CmpInstruction::LE, CmpInstruction::G, CmpInstruction::GE, CmpInstruction::E, CmpInstruction::NE};
        new CmpInstruction(cmpOps[op - LESS], dst, expr1->getOperand(), expr2->getOperand(), now_bb);
        /* true和false未知，interB已知
        cmp
        br true, interB

        interB:
        b false
        */
        if (this->isConde())
        {
            BasicBlock *interB;
            interB = new BasicBlock(now_func);
            true_list.push_back(new CondBrInstruction(nullptr, interB, dst, now_bb));
            false_list.push_back(new UncondBrInstruction(nullptr, interB));
        }
    }
    else if (op >= ADD && op <= MOD)
    {
        expr1->genCode();
        expr2->genCode();
        int opcodes[] = {BinaryInstruction::ADD, BinaryInstruction::SUB, BinaryInstruction::MUL, BinaryInstruction::DIV, BinaryInstruction::MOD};
        new BinaryInstruction(opcodes[op - ADD], dst, expr1->getOperand(), expr2->getOperand(), now_bb);
    }
}

void UnaryExpr::genCode()
{
    expr->genCode();
    if (op == SUB)
    {
        if (expr->getType()->isInt())
            new BinaryInstruction(BinaryInstruction::SUB, dst, new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0)), expr->getOperand(), now_bb);
        else if (expr->getType()->isFloat())
            new BinaryInstruction(BinaryInstruction::SUB, dst, new Operand(new ConstantSymbolEntry(TypeSystem::floatType, 0)), expr->getOperand(), now_bb);
    }
    else if (op == NOT)
    {
        if (expr->getType()->isInt())
        {
            new CmpInstruction(CmpInstruction::E, dst, expr->getOperand(), new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0)), now_bb);
        }
        else if (expr->getType()->isFloat())
        {
            new CmpInstruction(CmpInstruction::E, dst, expr->getOperand(), new Operand(new ConstantSymbolEntry(TypeSystem::floatType, 0)), now_bb);
        }
        else
        {
            new XorInstruction(dst, expr->getOperand(), now_bb);
        }
        if (isCond)
        {
            BasicBlock *interB;
            interB = new BasicBlock(now_func);
            true_list.push_back(new CondBrInstruction(nullptr, interB, dst, now_bb));
            false_list.push_back(new UncondBrInstruction(nullptr, interB));
        }
    }
}

void CallExpr::genCode()
{
    std::vector<Operand *> rParams;
    for (auto param : params)
    {
        param->genCode();
        rParams.push_back(param->getOperand());
    }
    new CallInstruction(dst, symbolEntry, rParams, now_bb);
}

void Constant::genCode()
{
    // we don't need to generate code.
}

void Id::genCode()
{
    IdentifierSymbolEntry *se = (IdentifierSymbolEntry *)symbolEntry;
    Operand *addr = se->getAddr();
    if (se->getType()->isArray())
    {
        std::vector<Operand *> offs;
        ExprNode *tmp = index;
        while (tmp)
        {
            tmp->genCode();
            offs.push_back(tmp->getOperand());
            tmp = (ExprNode *)tmp->getNext();
        }
        if (this->isPointer)
        {
            // 数组作为函数参数传递指针，取数组指针就行
            // 生成一条gep指令返回就行
            offs.push_back(new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0)));
            new GepInstruction(dst, addr, offs, now_bb);
            return;
        }
        if (((ArrayType *)se->getType())->getBaseType()->isInt())
            addr = new Operand(new TemporarySymbolEntry(new PointerType(TypeSystem::intType), SymbolTable::getLabel()));
        else if (((ArrayType *)se->getType())->getBaseType()->isFloat())
            addr = new Operand(new TemporarySymbolEntry(new PointerType(TypeSystem::floatType), SymbolTable::getLabel()));
        new GepInstruction(addr, se->getAddr(), offs, now_bb);
    }
    else if (se->getType()->isPtr())
    {
        ExprNode *tmp = index;
        if (tmp == nullptr)
        {
            // 如果数组标识符没有索引，他应该是作为参数传递的，取数组指针就行
            new LoadInstruction(dst, addr, now_bb);
            return;
        }
        Operand *base = new Operand(new TemporarySymbolEntry(((PointerType *)(addr->getType()))->getType(), SymbolTable::getLabel()));
        new LoadInstruction(base, addr, now_bb);
        std::vector<Operand *> offs;
        while (tmp)
        {
            tmp->genCode();
            offs.push_back(tmp->getOperand());
            tmp = (ExprNode *)tmp->getNext();
        }
        if (((ArrayType *)((PointerType *)se->getType())->getType())->getBaseType()->isInt())
            addr = new Operand(new TemporarySymbolEntry(new PointerType(TypeSystem::intType), SymbolTable::getLabel()));
        else if (((ArrayType *)((PointerType *)se->getType())->getType())->getBaseType()->isFloat())
            addr = new Operand(new TemporarySymbolEntry(new PointerType(TypeSystem::floatType), SymbolTable::getLabel()));
        new GepInstruction(addr, base, offs, now_bb, true);
    }
    new LoadInstruction(dst, addr, now_bb);
}

void ImplictCastExpr::genCode()
{
    expr->genCode();
    // bool转int
    if (op == BTI)
    {
        new ZextInstruction(dst, expr->getOperand(), true, now_bb);
    }
    // int转bool
    else if (op == ITB)
    {
        new CmpInstruction(CmpInstruction::NE, dst, expr->getOperand(), new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0)), now_bb);
    }
    else if (op == FTI)
    {
        new F2IInstruction(dst, expr->getOperand(), now_bb);
    }
    else if (op == ITF)
    {
        new I2FInstruction(dst, expr->getOperand(), now_bb);
    }
    else if (op == FTB)
    {
        new CmpInstruction(CmpInstruction::NE, dst, expr->getOperand(), new Operand(new ConstantSymbolEntry(TypeSystem::floatType, 0)), now_bb);
    }
    else if (op == BTF)
    {
        Operand *internal_op = new Operand(new TemporarySymbolEntry(TypeSystem::intType, SymbolTable::getLabel()));
        new ZextInstruction(internal_op, expr->getOperand(), true, now_bb);
        new I2FInstruction(dst, internal_op, now_bb);
    }
    if (this->isCond)
    {
        BasicBlock *interB;
        interB = new BasicBlock(now_func);
        true_list.push_back(new CondBrInstruction(nullptr, interB, dst, now_bb));
        false_list.push_back(new UncondBrInstruction(nullptr, interB));
    }
}

void CompoundStmt::genCode()
{
    // Todo
    if (stmt)
    {
        stmt->genCode();
    }
}

void SeqNode::genCode()
{
    // Todo
    stmt1->genCode();
    stmt2->genCode();
}

void DeclStmt::genCode()
{
    IdentifierSymbolEntry *se = dynamic_cast<IdentifierSymbolEntry *>(id->getSymPtr());
    if (se->isGlobal())
    {
        Operand *addr;
        SymbolEntry *addr_se;
        addr_se = new IdentifierSymbolEntry(*se);
        addr_se->setType(new PointerType(se->getType()));
        addr = new Operand(addr_se);
        se->setAddr(addr);
        unit.addGlobalVar(se);
        if (se->getType()->isArray() && exprArray)
        {
            int size = se->getType()->getSize() / TypeSystem::intType->getSize();
            double *arrayValue = new double[size];
            se->setArrayValue(arrayValue);
            for (int i = 0; i < size; i++)
            {
                if (exprArray[i])
                {
                    arrayValue[i] = exprArray[i]->getValue();
                }
                else
                {
                    arrayValue[i] = 0;
                }
            }
        }
    }
    else
    {
        BasicBlock *entry = now_func->getEntry();
        Instruction *alloca;
        Operand *addr;
        addr = new Operand(new TemporarySymbolEntry(new PointerType(se->getType()), SymbolTable::getLabel()));
        alloca = new AllocaInstruction(addr, se); // allocate space for local id in function stack.
        entry->insertFront(alloca);               // allocate instructions should be inserted into the begin of the entry block.
        se->setAddr(addr);                        // set the addr operand in symbol entry so that we can use it in subsequent code generation.
        if (expr)
        {
            expr->genCode();
            new StoreInstruction(addr, expr->getOperand(), now_bb);
        }
        if (se->isParam())
        {
            now_func->addParam(se->getArgAddr());
            new StoreInstruction(addr, se->getArgAddr(), now_bb);
        }
        if (se->getType()->isArray() && exprArray) // 如果数组有初始化
        {
            Type *eleType = ((ArrayType *)se->getType())->getBaseType();
            Type *baseType = eleType->isFloat() ? TypeSystem::floatType : TypeSystem::intType;
            std::vector<int> indexs = ((ArrayType *)se->getType())->getIndexs();
            int size = se->getType()->getSize() / TypeSystem::intType->getSize();
            std::vector<Operand *> offs;
            for (int j = 0; j < indexs.size(); j++)
            {
                offs.push_back(new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0)));
            }
            indexs = ((ArrayType *)se->getType())->getIndexs();
            // 因为数组初始化可能会用到很多零，这里我们先准备一个，然后之后就不用频繁load了
            Operand *zeroReg = new Operand(new TemporarySymbolEntry(baseType, SymbolTable::getLabel()));
            Operand *zero = new Operand(new ConstantSymbolEntry(baseType, 0));
            new BinaryInstruction(BinaryInstruction::ADD, zeroReg, zero, zero, now_bb);
            Operand *ele_addr = new Operand(new TemporarySymbolEntry(new PointerType(new ArrayType({}, baseType)), SymbolTable::getLabel()));
            Operand *step = new Operand(new ConstantSymbolEntry(TypeSystem::intType, 1));
            new GepInstruction(ele_addr, se->getAddr(), offs, now_bb);
            for (int i = 0; i < size; i++)
            {
                if (exprArray[i])
                {
                    exprArray[i]->genCode();
                    new StoreInstruction(ele_addr, exprArray[i]->getOperand(), now_bb);
                }
                else
                {
                    new StoreInstruction(ele_addr, zeroReg, now_bb);
                }
                if (i != size - 1)
                {
                    Operand *next_addr = new Operand(new TemporarySymbolEntry(new PointerType(new ArrayType({}, eleType)), SymbolTable::getLabel()));
                    new GepInstruction(next_addr, ele_addr, {step}, now_bb, true);
                    ele_addr = next_addr;
                }
                // new BinaryInstruction(BinaryInstruction::ADD, ele_addr, ele_addr, step, now_bb);
            }
            // for (int i = 0; i < size; i++) {
            //     if (exprArray[i]) {
            //         std::cout  << i << " " << exprArray[i]->getSymPtr()->toStr() << " ";
            //     }
            // }
            // std::cout << std::endl;
        }
    }
    // 这里要看看下一个有没有
    if (this->getNext())
    {
        this->getNext()->genCode();
    }
}

void DeclStmt::setInitArray(ExprNode **exprArray)
{
    // 能走到这一步，id就是个数组
    Type *idBaseType = ((ArrayType *)id->getSymPtr()->getType())->getBaseType();
    this->exprArray = exprArray;
    int size = id->getSymPtr()->getType()->getSize() / TypeSystem::intType->getSize();
    for (int i = 0; i < size; i++)
    {
        if (this->exprArray[i])
        {
            // 这里考虑这一种就行，因为sysy里面整形数组初始元素只能是整数，但是浮点数不是
            if (this->exprArray[i]->getType()->isInt() && idBaseType->isFloat())
            {
                this->exprArray[i] = new ImplictCastExpr(this->exprArray[i], ImplictCastExpr::ITF);
            }
        }
    }
}

void BlankStmt::genCode()
{
}

void IfStmt::genCode()
{
    BasicBlock *then_bb, *end_bb;

    then_bb = new BasicBlock(now_func);
    end_bb = new BasicBlock(now_func);

    cond->genCode();
    backPatch(cond->trueList(), then_bb);
    backPatch(cond->falseList(), end_bb);

    builder->setInsertBB(then_bb);
    thenStmt->genCode();
    then_bb = builder->getInsertBB();
    new UncondBrInstruction(end_bb, then_bb);

    builder->setInsertBB(end_bb);
}

void IfElseStmt::genCode()
{
    // Todo
    BasicBlock *then_bb, *else_bb, *end_bb;

    then_bb = new BasicBlock(now_func);
    else_bb = new BasicBlock(now_func);
    end_bb = new BasicBlock(now_func);

    // 生成cond
    cond->genCode();
    backPatch(cond->trueList(), then_bb);
    backPatch(cond->falseList(), else_bb);

    // 生成then
    builder->setInsertBB(then_bb);
    thenStmt->genCode();
    then_bb = builder->getInsertBB();
    new UncondBrInstruction(end_bb, then_bb);

    // 生成else
    builder->setInsertBB(else_bb);
    elseStmt->genCode();
    else_bb = builder->getInsertBB();
    new UncondBrInstruction(end_bb, else_bb);

    builder->setInsertBB(end_bb);
}

void WhileStmt::genCode()
{
    BasicBlock *cond_bb, *stmt_bb, *end_bb;
    cond_bb = new BasicBlock(now_func);
    stmt_bb = new BasicBlock(now_func);
    end_bb = new BasicBlock(now_func);
    this->cond_bb = cond_bb;
    this->end_bb = end_bb;

    new UncondBrInstruction(cond_bb, now_bb);
    builder->setInsertBB(cond_bb);
    cond->genCode();
    backPatch(cond->trueList(), stmt_bb);
    backPatch(cond->falseList(), end_bb);

    builder->setInsertBB(stmt_bb);
    stmt->genCode();
    stmt_bb = builder->getInsertBB();
    new UncondBrInstruction(cond_bb, stmt_bb);

    builder->setInsertBB(end_bb);
}

void BreakStmt::genCode()
{
    if (whileStmt)
    {
        new UncondBrInstruction(((WhileStmt *)whileStmt)->get_end_bb(), now_bb);
        builder->setInsertBB(new BasicBlock(now_func));
    }
}

void ContinueStmt::genCode()
{
    if (whileStmt)
    {
        new UncondBrInstruction(((WhileStmt *)whileStmt)->get_cond_bb(), now_bb);
        builder->setInsertBB(new BasicBlock(now_func));
    }
}

void ReturnStmt::genCode()
{
    // Todo
    if (retValue)
    {
        retValue->genCode();
    }
    new RetInstruction(retValue ? retValue->getOperand() : nullptr, now_bb);
}

void AssignStmt::genCode()
{
    IdentifierSymbolEntry *se = (IdentifierSymbolEntry *)lval->getSymPtr();
    expr->genCode();
    Operand *addr = se->getAddr();
    Operand *src = expr->getOperand();
    /***
     * We haven't implemented array yet, the lval can only be ID. So we just store the result of the `expr` to the addr of the id.
     * If you want to implement array, you have to caculate the address first and then store the result into it.
     */
    if (se->getType()->isArray())
    {
        // 先算地址
        ExprNode *index = ((Id *)lval)->getIndex();
        std::vector<Operand *> offs;
        while (index)
        {
            index->genCode();
            offs.push_back(index->getOperand());
            index = (ExprNode *)index->getNext();
        }
        if (((ArrayType *)se->getType())->getBaseType()->isInt())
            addr = new Operand(new TemporarySymbolEntry(new PointerType(TypeSystem::intType), SymbolTable::getLabel()));
        else if (((ArrayType *)se->getType())->getBaseType()->isFloat())
            addr = new Operand(new TemporarySymbolEntry(new PointerType(TypeSystem::floatType), SymbolTable::getLabel()));
        new GepInstruction(addr, se->getAddr(), offs, now_bb);
    }
    else if (se->getType()->isPtr())
    {
        Operand *base = new Operand(new TemporarySymbolEntry(((PointerType *)(addr->getType()))->getType(), SymbolTable::getLabel()));
        new LoadInstruction(base, addr, now_bb);
        ExprNode *tmp = ((Id *)lval)->getIndex();
        std::vector<Operand *> offs;
        while (tmp)
        {
            tmp->genCode();
            offs.push_back(tmp->getOperand());
            tmp = (ExprNode *)tmp->getNext();
        }
        if (((ArrayType *)(((PointerType *)se->getType())->getType()))->getBaseType()->isInt())
            addr = new Operand(new TemporarySymbolEntry(new PointerType(TypeSystem::intType), SymbolTable::getLabel()));
        if (((ArrayType *)(((PointerType *)se->getType())->getType()))->getBaseType()->isFloat())
            addr = new Operand(new TemporarySymbolEntry(new PointerType(TypeSystem::floatType), SymbolTable::getLabel()));
        new GepInstruction(addr, base, offs, now_bb, true);
    }
    new StoreInstruction(addr, src, now_bb);
}

void ExprStmt::genCode()
{
    expr->genCode();
}

void FunctionDef::genCode()
{
    Unit *unit = builder->getUnit();
    Function *func = new Function(unit, se);
    BasicBlock *entry = func->getEntry();
    // set the insert point to the entry basicblock of this function.
    builder->setInsertBB(entry);

    if (decl != nullptr)
    {
        decl->genCode();
    }
    stmt->genCode();

    /**
     * Construct control flow graph. You need do set successors and predecessors for each basic block.
     * Todo
     */
    for (auto b = func->begin(); b != func->end(); b++)
    {
        Instruction *lastIns = (*b)->rbegin();
        if (lastIns->isCond())
        {
            BasicBlock *trueBranch = ((CondBrInstruction *)lastIns)->getTrueBranch();
            BasicBlock *falseBranch = ((CondBrInstruction *)lastIns)->getFalseBranch();
            (*b)->addSucc(trueBranch);
            (*b)->addSucc(falseBranch);
            trueBranch->addPred(*b);
            falseBranch->addPred(*b);
        }
        else if (lastIns->isUncond())
        {
            BasicBlock *branch = ((UncondBrInstruction *)lastIns)->getBranch();
            (*b)->addSucc(branch);
            branch->addPred(*b);
        }
        else if (!lastIns->isRet())
        { // 如果这个块没有后继，而且没有ret
            if (((FunctionType *)now_func->getSymPtr()->getType())->getRetType()->isInt())
            {
                new RetInstruction(new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0)), (*b));
            }
            if (((FunctionType *)now_func->getSymPtr()->getType())->getRetType()->isFloat())
            {
                new RetInstruction(new Operand(new ConstantSymbolEntry(TypeSystem::floatType, 0)), (*b));
            }
            else if (((FunctionType *)now_func->getSymPtr()->getType())->getRetType() == TypeSystem::voidType)
            {
                new RetInstruction(nullptr, (*b));
            }
        }
    }
}

// typeCheck代码区

void Ast::typeCheck()
{
    if (root != nullptr)
        return root->typeCheck();
}

void BinaryExpr::typeCheck()
{
}

void UnaryExpr::typeCheck()
{
}

void CallExpr::typeCheck()
{
}

void Constant::typeCheck()
{
}

void Id::typeCheck()
{
}

void CompoundStmt::typeCheck()
{
}

void SeqNode::typeCheck()
{
}

void DeclStmt::typeCheck()
{
}

void IfStmt::typeCheck()
{
}

void IfElseStmt::typeCheck()
{
}

void WhileStmt::typeCheck()
{
}

void ReturnStmt::typeCheck()
{
}

void AssignStmt::typeCheck()
{
}

void ExprStmt::typeCheck()
{
}

void FunctionDef::typeCheck()
{
}

// output代码区

void Ast::output()
{
    fprintf(yyout, "program\n");
    if (root != nullptr)
        root->output(4);
}

void ExprNode::output(int level)
{
    std::string name, type;
    name = symbolEntry->toStr();
    type = symbolEntry->getType()->toStr();
    fprintf(yyout, "%*cconst string\ttype:%s\t%s\n", level, ' ', type.c_str(), name.c_str());
}

void BinaryExpr::output(int level)
{
    std::string op_str;
    switch (op)
    {
    case ADD:
        op_str = "add";
        break;
    case SUB:
        op_str = "sub";
        break;
    case MUL:
        op_str = "mul";
        break;
    case DIV:
        op_str = "div";
        break;
    case MOD:
        op_str = "mod";
        break;
    case AND:
        op_str = "and";
        break;
    case OR:
        op_str = "or";
        break;
    case LESS:
        op_str = "less";
        break;
    case LESSEQUAL:
        op_str = "lessequal";
        break;
    case GREATER:
        op_str = "greater";
        break;
    case GREATEREQUAL:
        op_str = "greaterequal";
        break;
    case EQUAL:
        op_str = "equal";
        break;
    case NOTEQUAL:
        op_str = "notequal";
        break;
    }
    fprintf(yyout, "%*cBinaryExpr\top: %s\ttype: %s\n", level, ' ',
            op_str.c_str(), type->toStr().c_str());
    expr1->output(level + 4);
    expr2->output(level + 4);
}

void UnaryExpr::output(int level)
{
    std::string op_str;
    switch (op)
    {
    case NOT:
        op_str = "not";
        break;
    case SUB:
        op_str = "minus";
        break;
    }
    fprintf(yyout, "%*cUnaryExpr\top: %s\ttype: %s\n", level, ' ',
            op_str.c_str(), type->toStr().c_str());
    expr->output(level + 4);
}

void CallExpr::output(int level)
{
    std::string name, type;
    int scope;
    if (symbolEntry)
    {
        name = symbolEntry->toStr();
        type = symbolEntry->getType()->toStr();
        scope = dynamic_cast<IdentifierSymbolEntry *>(symbolEntry)->getScope();
        fprintf(yyout, "%*cCallExpr\tfunction name: %s\tscope: %d\ttype: %s\n", level, ' ', name.c_str(), scope, type.c_str());
        // 打印参数信息
        for (auto param : params)
        {
            param->output(level + 4);
        }
    }
}

void Constant::output(int level)
{
    std::string type, value;
    type = symbolEntry->getType()->toStr();
    value = symbolEntry->toStr();
    fprintf(yyout, "%*cIntegerLiteral\tvalue: %s\ttype: %s\n", level, ' ',
            value.c_str(), type.c_str());
}

void Id::output(int level)
{
    std::string name, type;
    int scope;
    name = symbolEntry->toStr();
    type = symbolEntry->getType()->toStr();
    scope = dynamic_cast<IdentifierSymbolEntry *>(symbolEntry)->getScope();
    fprintf(yyout, "%*cId\tname: %s\tscope: %d\ttype: %s\n", level, ' ', name.c_str(), scope, type.c_str());
}

double ImplictCastExpr::getValue()
{
    double value = expr->getValue();
    switch (op)
    {
    case ITB:
    case FTB:
        value = (!!value);
        break;
    case FTI:
        value = (int)value;
        break;
    case BTI:
    case ITF:
    case BTF:
    default:
        break;
    }
    return value;
}

void ImplictCastExpr::output(int level)
{
    fprintf(yyout, "%*cImplictCastExpr\ttype: %s to %s\n", level, ' ', expr->getType()->toStr().c_str(), type->toStr().c_str());
    this->expr->output(level + 4);
}

void CompoundStmt::output(int level)
{
    fprintf(yyout, "%*cCompoundStmt\n", level, ' ');
    stmt->output(level + 4);
}

void SeqNode::output(int level)
{
    stmt1->output(level);
    stmt2->output(level);
}

DeclStmt::DeclStmt(Id *id, ExprNode *expr) : id(id), expr(expr)
{
    this->exprArray = nullptr;
    if (expr)
    {
        if (id->getType()->isFloat() && expr->getType()->isInt())
            this->expr = new ImplictCastExpr(expr, ImplictCastExpr::ITF);
        if (id->getType()->isInt() && expr->getType()->isFloat())
            this->expr = new ImplictCastExpr(expr, ImplictCastExpr::FTI);
    }
};

void DeclStmt::output(int level)
{
    fprintf(yyout, "%*cDeclStmt\n", level, ' ');
    id->output(level + 4);
    if (expr)
    {
        expr->output(level + 4);
    }
    if (this->getNext())
    {
        ((DeclStmt *)getNext())->output(level);
    }
}

void BlankStmt::output(int level)
{
    fprintf(yyout, "%*cBlankStmt\n", level, ' ');
}

void IfStmt::output(int level)
{
    fprintf(yyout, "%*cIfStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
}

void IfElseStmt::output(int level)
{
    fprintf(yyout, "%*cIfElseStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
    elseStmt->output(level + 4);
}

void WhileStmt::output(int level)
{
    fprintf(yyout, "%*cWhileStmt\n", level, ' ');
    cond->output(level + 4);
    stmt->output(level + 4);
}
void BreakStmt::output(int level)
{
    fprintf(yyout, "%*cBreakStmt\n", level, ' ');
}

void ContinueStmt::output(int level)
{
    fprintf(yyout, "%*cContinueStmt\n", level, ' ');
}

void ReturnStmt::output(int level)
{
    fprintf(yyout, "%*cReturnStmt\n", level, ' ');
    if (retValue != nullptr)
    {
        retValue->output(level + 4);
    }
}

void AssignStmt::output(int level)
{
    fprintf(yyout, "%*cAssignStmt\n", level, ' ');
    lval->output(level + 4);
    expr->output(level + 4);
}

void ExprStmt::output(int level)
{
    fprintf(yyout, "%*cExprStmt\n", level, ' ');
    expr->output(level + 4);
}

void FunctionDef::output(int level)
{
    std::string name, type;
    name = se->toStr();
    type = se->getType()->toStr();
    fprintf(yyout, "%*cFunctionDefine function name: %s, type: %s\n", level, ' ',
            name.c_str(), type.c_str());
    if (decl)
    {
        decl->output(level + 4);
    }
    stmt->output(level + 4);
}

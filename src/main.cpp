#include <iostream>
#include <string.h>
#include <unistd.h>
#include "Ast.h"
#include "Unit.h"
#include "SymbolTable.h"
#include "MachineCode.h"
#include "LinearScan.h"
#include "MachinePeepHole.h"
#include "MachineStraight.h"
#include "Mem2Reg.h"
#include "SimplifyCFG.h"
#include "IRSparseCondConstProp.h"
#include "PureFunctionAnalyser.h"
#include "IRComSubExprElim.h"
#include "PhiElim.h"
#include "DeadCodeElimination.h"
#include "LoopCodeMotion.h"
#include "MachineCopyProp.h"
#include "Global2Local.h"
#include "MachineDeadCodeElim.h"
#include "FunctionInline.h"
#include "TailCallAnalyser.h"
#include "MachineTailCallHandler.h"
#include "GraphColor.h"

using namespace std;

Ast ast;
Unit unit;
MachineUnit mUnit;
extern FILE *yyin;
extern FILE *yyout;

int yyparse();

char outfile[256] = "a.out";
bool dump_tokens;
bool dump_ast;
bool dump_ir;
bool dump_asm;
bool optmize = false;

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "Siato:O::")) != -1)
    {
        switch (opt)
        {
        case 'o':
            strcpy(outfile, optarg);
            break;
        case 'a':
            dump_ast = true;
            break;
        case 't':
            dump_tokens = true;
            break;
        case 'i':
            dump_ir = true;
            break;
        case 'S':
            dump_asm = true;
            break;
        case 'O': // 先默认都优化吧
            optmize = true;
            break;
        default:
            fprintf(stderr, "Usage: %s [-o outfile] infile\n", argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }
    if (optind >= argc)
    {
        fprintf(stderr, "no input file\n");
        exit(EXIT_FAILURE);
    }
    if (!(yyin = fopen(argv[optind], "r")))
    {
        fprintf(stderr, "%s: No such file or directory\nno input file\n", argv[optind]);
        exit(EXIT_FAILURE);
    }
    if (!(yyout = fopen(outfile, "w")))
    {
        fprintf(stderr, "%s: fail to open output file\n", outfile);
        exit(EXIT_FAILURE);
    }

    // dump_tokens = true;
    yyparse();
    Log("语法分析成功\n");
    if (dump_ast)
        ast.output();
    ast.typeCheck();
    ast.genCode(&unit);
    Log("中间代码生成成功\n");

    SimplifyCFG spcfg(&unit);
    Global2Local g2l(&unit);
    Mem2Reg m2r(&unit);
    IRSparseCondConstProp sccp(&unit);
    IRComSubExprElim cse(&unit);
    DeadCodeElimination dce(&unit);
    PhiElimination pe(&unit);
    LoopCodeMotion lcm(&unit);
    FunctionInline finline(&unit);
    TailCallAnalyser tca(&unit);

    // g2l.pass();
    m2r.pass(); // Only IR supported
    spcfg.pass();
    sccp.pass();
    cse.pass();
    sccp.pass();
    cse.pass();
    dce.pass();
    finline.pass();
    sccp.pass();
    cse.pass();
    sccp.pass();
    cse.pass();
    // dce.pass();
    lcm.pass();
    pe.pass();
    tca.pass();

    Log("IR优化成功"); /**/

    if (dump_ir)
        unit.output();
    unit.genMachineCode(&mUnit);
    Log("目标代码生成成功");
    MachinePeepHole mph(&mUnit, 2);
    MachineStraight mst(&mUnit);
    MachineCopyProp mcp(&mUnit);
    MachineDeadCodeElim mdce(&mUnit);
    MachineTailCallHandler mtch(&mUnit);
    mst.pass();
    mph.pass();
    mcp.pass();
    mph.pass();
    mdce.pass();
    mst.pass();
    mtch.pass(); // 把这个放在最后做，要不大概率会有问题
    Log("目标代码优化成功");
    // if (optmize)
    // {
    GraphColor graphColor(&mUnit);
    graphColor.allocateRegisters();
    // }
    // else
    // {
    // LinearScan linearScan(&mUnit);
    // linearScan.allocateRegisters();
    // }
    Log("寄存器分配完成\n");
    mph.pass();
    mst.pass();
    if (dump_asm)
        mUnit.output();
    return 0;
}

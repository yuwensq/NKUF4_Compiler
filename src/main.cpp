#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sstream>
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
#include "MachineLVN.h"
#include "IRPeepHole.h"
#include "GVN.h"
#include "IRDefUseCheck.h"

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
    IRPeepHole iph(&unit);
    GlobalValueNumbering gvn(&unit);
    DefUseCheck duc(&unit);

    auto atomicCodeElim = [&]()
    {
        static int turn = 1;
        stringstream ss;
        ss << turn;
        std::string num;
        ss >> num;
        spcfg.pass();
        // duc.pass("spcfg" + num);
        iph.pass();
        // duc.pass("iph" + num);
        sccp.pass();
        // duc.pass("sccp" + num);
        cse.pass();
        // duc.pass("cse" + num);
        // gvn.pass();
        // duc.pass("gvn" + num);
        dce.pass();
        // duc.pass("dce" + num);
        turn++;
    };

    auto pairCodeElim = [&]()
    {
        atomicCodeElim();
        atomicCodeElim();
    };

    g2l.pass();
    // duc.pass("g2l");
    m2r.pass(); // Only IR supported
    // duc.pass("m2r");
    pairCodeElim();
    finline.pass();
    // duc.pass("func inline");
    pairCodeElim();
    spcfg.pass();
    if (optmize)
        lcm.pass(); // 代码外提
    pairCodeElim();
    lcm.pass2(); // 强度削弱
    pairCodeElim();
    lcm.pass1();
    // if (optmize) // 功能测试不开这个，这个会让某些样例很慢
    // {
    //     do
    //     {
    //         pairCodeElim();
    //     } while (lcm.pass1()); // 循环展开
    // }
    // pairCodeElim();
    // if (optmize)
    //     lcm.pass();
    // pairCodeElim();
    // pe.pass();
    iph.pass2();
    tca.pass();

    Log("IR优化成功"); /**/

    if (dump_ir)
        unit.output();
    if (dump_asm)
    {
        unit.genMachineCode(&mUnit);
        Log("目标代码生成成功");
        MachinePeepHole mph(&mUnit);
        MachineStraight mst(&mUnit);
        MachineCopyProp mcp(&mUnit);
        MachineDeadCodeElim mdce(&mUnit);
        MachineLVN mlvn(&mUnit);
        MachineTailCallHandler mtch(&mUnit);
        mst.pass();
        mlvn.pass();
        mcp.pass();
        if (optmize) // shm写的活跃变量分析太拉了，这个开了long_line超时
            mdce.pass();
        mph.pass();
        mcp.pass();
        mlvn.pass();
        mcp.pass();
        mph.pass();
        if (optmize) // shm写的活跃变量分析太拉了，这个开了long_line超时
            mdce.pass();
        mph.pass();
        mlvn.pass();
        if (optmize)
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
        mph.pass(true);
        mst.pass();
        mst.lastPass(); // 这个放在最后做

        mUnit.output();
    }
    return 0;
}

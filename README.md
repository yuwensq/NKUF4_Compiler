# NKUF4团队的SYSY编译器

```
团队学校：南开大学
团队名称：NKUF4
团队成员：徐文斌 许友锐 栗心武 聂志强
指导教师：王刚 李忠伟
```

## 项目简介

该项目为一个单前端单后端的编译器，可对[SysY语言](https://gitlab.eduxiji.net/nscscc/compiler2023/-/blob/master/SysY2022%E8%AF%AD%E8%A8%80%E5%AE%9A%E4%B9%89-V1.pdf)进行编译，生成armv7a架构的汇编语言。
![项目架构](https://raw.githubusercontent.com/yuwensq/imgBase/master/202308222049865.jpg)

项目总体上分为前端、中端和后端三部分。前端的词法分析和语法分析基于lex和yacc实现，中间代码与llvm IR兼容，后端生成arm汇编代码。
编译器实现了各种代码优化算法，具有较好的优化能力。
![](https://raw.githubusercontent.com/yuwensq/imgBase/master/202308222050780.jpg)

## 编译器命令
```
Usage：build/compiler [options] infile
Options:
    -o <file>   Place the output into <file>.
    -t          Print tokens.
    -a          Print abstract syntax tree.
    -i          Print intermediate code
    -S          Print assembly code
    -O2         open O2 optimization
```

## Makefile使用

* 编译：

```
    make
```
编译出我们的编译器。

* 运行：
```
    make run
```
以example.sy文件为输入，输出相应的汇编代码到example.s文件中。

* 批量测试：
```
    make test
```
对TEST_PATH目录下的每个.sy文件，编译器将其编译成汇编代码.s文件， 再使用gcc将.s文件汇编成二进制文件后执行， 将得到的输出与标准输出对比， 验证编译器实现的正确性。错误信息描述如下：
|  错误信息   | 描述  |
|  ----  | ----  |
| Compile Timeout  | 编译超时， 可能是编译器实现错误导致， 也可能是源程序过于庞大导致(可调整超时时间) |
| Compile Error  | 编译错误， 源程序有错误或编译器实现错误 |
|Assemble Error| 汇编错误， 编译器生成的目标代码不能由gcc正确汇编|
| Execute Timeout  |执行超时， 可能是编译器生成了错误的目标代码|
|Execute Error|程序运行时崩溃， 可能原因同Execute Timeout|
|Wrong Answer|答案错误， 执行程序得到的输出与标准输出不同|

具体的错误信息可在对应的.log文件中查看。

* GCC Assembly Code
```
    make gccasm
```
使用gcc编译器生成汇编代码。

* 清理:
```
    make clean
```

## 往届学长优秀作品参考
- [shm学长"NKUER4"团队](https://github.com/shm0214/2022_compile)
- [syd学长"天津泰达"团队](https://github.com/shm0214/2022_compile)
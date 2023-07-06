#ifndef __TAIL_CALL_ANALYSER_H_
#define __TAIL_CALL_ANALYSER_H_

#include "Unit.h"

// 尾调用分析，在中间代码优化的最后做就行了
class TailCallAnalyser
{
private:
    Unit *unit;

public:
    TailCallAnalyser(Unit *unit) : unit(unit){};
    // 这个pass主要用于标记尾调用，
    // 实际的优化在中间代码到汇编代码翻译的时候做
    void pass();
};

#endif
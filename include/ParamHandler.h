#ifndef __PARAMHANDLER_H__
#define __PARAMHANDLER_H__

#include "Unit.h"

// 这里再写一个类吧，用于处理mem2reg后的参数怎么存的问题，这样写
// 方便后边更换其他的参数方法

class ParamHandler
{
private:
    Unit *unit;
    void process(Function*);

public:
    ParamHandler(Unit *unit) : unit(unit){};
    void pass();
};

#endif
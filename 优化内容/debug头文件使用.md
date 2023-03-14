# PA日志宏

debug里定义了一个Assert宏和Log宏。两个宏会将调试信息输出至测试样例对应的log文件之中。

Assert宏的用法如下，cond为判断条件，如果cond不满足，后边的字符串就会被输出。
```cpp
Assert(cond, "...");
// 具体为如下样式
Assert(a == nullptr, "%s", "a为空");
```

Log宏的用法如下，Log就相当于`Assert(true, "...")`
```cpp
Log("...");
// 具体为如下样式
Log("%s", "代码生成成功");
```

除了上述两个宏，还有panic宏和TODO()宏，看源代码很好理解。
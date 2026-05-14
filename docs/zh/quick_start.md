# 快速入门<a name="ZH-CN_TOPIC_0000002522064774"></a>

以Hash算法为例，以下示例代码演示了如何调用多模规则匹配动态库各个函数接口。

```
#include <mmpm.h>
int main() {
handler;    mmpm_handler_t handler = {0};
mmpm_total_rules"abc";    mmpm_rule_t mmpm_total_rules = {1, 3, "abc"};
    mmpm_rule_t mmpm_target_rules = {0, 3, "abc"};
    // 规则匹配引擎初始化，并指定使用Hash算法
    mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 100000, 96);
    // 向规则匹配引擎添加待匹配规则
    mmpm_add_rule(&handler, &mmpm_total_rules, 1);
    // 向规则匹配引擎进行规则匹配
    mmpm_find_rule(&handler, &mmpm_target_rules);
    // 从规则匹配引擎里删除指定规则
    mmpm_remove_rule(&handler, &mmpm_total_rules, 1);
    // 销毁多模规则匹配引擎
    mmpm_handler_destroy(&handler);
    return 0;
}
```

编译方式，能成功编译且正常执行，代表MMPM使能成功。

```
gcc your_program.c -o your_program -lmmpm
```


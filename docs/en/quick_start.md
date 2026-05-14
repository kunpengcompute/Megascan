# Quick Start<a name="EN-US_TOPIC_0000002522064774"></a>

The following example code demonstrates how to call each function API of the multi-pattern rule matching dynamic library when using a hash algorithm.

```
#include <mmpm.h>
int main() {
handler;    mmpm_handler_t handler = {0};
mmpm_total_rules"abc";    mmpm_rule_t mmpm_total_rules = {1, 3, "abc"};
    mmpm_rule_t mmpm_target_rules = {0, 3, "abc"};
    // Initialize a rule matching engine and specify the hash algorithm.
    mmpm_handler_init(&handler, MMPM_MATCHER_TYPE_HASH, 100000, 96);
    // Add rules to the rule matching engine.
    mmpm_add_rule(&handler, &mmpm_total_rules, 1);
    // Perform rule matching using the rule matching engine.
    mmpm_find_rule(&handler, &mmpm_target_rules);
    // Remove a specified rule from the rule matching engine.
    mmpm_remove_rule(&handler, &mmpm_total_rules, 1);
    // Destroy the multi-pattern rule matching engine.
    mmpm_handler_destroy(&handler);
    return 0;
}
```

Successful compilation and normal execution indicate that MMPM has been successfully enabled.

```
gcc your_program.c -o your_program -lmmpm
```


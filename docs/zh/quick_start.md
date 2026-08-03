# 快速入门

## 前提条件

请先按照[《安装指南》](./installation_guide.md)完成源码编译或RPM安装。本示例使用AC算法的Block模式，演示规则编译、scratch申请、数据扫描和资源释放。

## 操作步骤

### 1. 准备示例程序

在Megascan源码根目录创建`quick_start.c`：

```c
#include <stdint.h>
#include <stdio.h>
#include <ms.h>

int main(void)
{
    const char *expressions[] = {"abc", "def", "101&102"};
    uint32_t flags[] = {
        MS_FLAG_QUIET,
        MS_FLAG_QUIET,
        MS_FLAG_COMBINATION
    };
    uint32_t rule_ids[] = {101, 102, 1001};
    uint32_t matched_ids[8] = {0};
    uint32_t matched_count = 0;
    ms_database_t *db = NULL;
    ms_scratch_t *scratch = NULL;
    int ret;

    // 编译AC规则并生成database
    ret = ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
        expressions, flags, rule_ids, 3);
    if (ret != MS_SUCCESS) {
        goto cleanup;
    }

    // 根据database申请扫描所需的scratch
    ret = ms_alloc_scratch(db, &scratch);
    if (ret != MS_SUCCESS) {
        goto cleanup;
    }

    // 使用Block模式扫描输入数据
    ret = ms_scan_database(db, (const uint8_t *)"abcdef", 6, scratch,
        matched_ids, &matched_count, 8);
    if (ret != MS_SUCCESS) {
        goto cleanup;
    }

    // 打印命中的规则ID
    for (uint32_t i = 0; i < matched_count; i++) {
        printf("matched id: %u\n", matched_ids[i]);
    }

cleanup:
    if (scratch != NULL) {
        ms_free_scratch(scratch);
    }
    if (db != NULL) {
        ms_free_database(db);
    }
    return ret == MS_FIND_FAIL ? 0 : ret;
}
```

普通规则`abc`和`def`设置`MS_FLAG_QUIET`，参与组合逻辑但不单独上报。组合规则`101&102`引用规则ID；输入同时命中两条普通规则时，上报组合规则ID `1001`。

### 2. 编译Megascan

源码构建场景下，执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

确认动态库和头文件已经生成：

```bash
test -f output/lib/libms.so && echo "libms.so: OK"
test -f output/include/ms.h && echo "ms.h: OK"
```

回显示例：

```text
libms.so: OK
ms.h: OK
```

### 3. 编译示例程序

使用源码构建产物：

```bash
gcc quick_start.c -o quick_start \
  -I./output/include \
  -L./output/lib \
  -Wl,-rpath,"$PWD/output/lib" \
  -lms -pthread
```

使用RPM安装的头文件和动态库：

```bash
gcc quick_start.c -o quick_start \
  -I/usr/local/include \
  -L/usr/local/lib \
  -lms -pthread
```

### 4. 运行并检查结果

```bash
./quick_start
```

预期输出：

```text
matched id: 1001
```

程序退出码应为`0`：

```bash
echo $?
```

如果运行时提示找不到`libms.so`，源码构建场景可临时设置：

```bash
export LD_LIBRARY_PATH="$PWD/output/lib:${LD_LIBRARY_PATH:-}"
```

## 后续使用

Stream模式在database和scratch之外创建stream，并按照实际输入分片重复调用`ms_scan_stream()`。流结束时调用`ms_flush_stream()`或`ms_close_stream()`。接口参数、返回值和使用约束参见[《API参考》](./api_reference.md)。

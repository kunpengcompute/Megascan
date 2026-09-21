# Quick Start

<!-- md-trans-meta sourceCommit=cef0cf22901fa9ada7db6826684488ff749a38f9 translatedAt=2026-08-31T01:41:55.205Z pushedAt=2026-09-04T09:34:20.463Z -->

## Prerequisites

You have completed source code compilation or RPM package installation by following instructions in [Installation Guide](./installation_guide.md). This example uses the block mode of the AC algorithm to demonstrate rule compilation, scratch allocation, data scanning, and resource release.

## Procedure

### 1. Preparing the Example Program

Create `quick_start.c` in the root directory of the Megascan source code.

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

    // Compile the AC rules and generate the database.
    ret = ms_compile_database_multi(&db, MS_MATCHER_TYPE_AC, 0, 0,
        expressions, flags, rule_ids, 3);
    if (ret != MS_SUCCESS) {
        goto cleanup;
    }

    // Allocate the scratch required for scanning based on the database.
    ret = ms_alloc_scratch(db, &scratch);
    if (ret != MS_SUCCESS) {
        goto cleanup;
    }

    // Scan the input data in block mode.
    ret = ms_scan_database(db, (const uint8_t *)"abcdef", 6, scratch,
        matched_ids, &matched_count, 8);
    if (ret != MS_SUCCESS) {
        goto cleanup;
    }

    // Print the matched rule IDs.
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

The basic rules `abc` and `def` are marked with `MS_FLAG_QUIET`, meaning they participate in the composite logic but are not reported individually. The composite rule `101&102` references the rule IDs `101` and `102` (the IDs of the two basic rules). When the input matches the two basic rules simultaneously, the composite rule ID `1001` is reported.

### 2. Compiling Megascan

In the source code build scenario, run the following commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Confirm that the dynamic library and header file have been generated.

```bash
test -f output/lib/libms.so && echo "libms.so: OK"
test -f output/include/ms.h && echo "ms.h: OK"
```

An example of command output is as follows:

```text
libms.so: OK
ms.h: OK
```

### 3. Compiling the Example Program

Use the outputs from the source code build:

```bash
gcc quick_start.c -o quick_start \
  -I./output/include \
  -L./output/lib \
  -Wl,-rpath,"$PWD/output/lib" \
  -lms -pthread
```

Use the header files and dynamic library installed via the RPM package:

```bash
gcc quick_start.c -o quick_start \
  -I/usr/local/include \
  -L/usr/local/lib \
  -lms -pthread
```

### 4. Running the Program and Checking the Result

```bash
./quick_start
```

The expected output is as follows:

```text
matched id: 1001
```

The program exit code should be `0`.

```bash
echo $?
```

If a prompt indicates that `libms.so` cannot be found at runtime, temporarily set the environment variable as follows in the source code build scenario:

```bash
export LD_LIBRARY_PATH="$PWD/output/lib:${LD_LIBRARY_PATH:-}"
```

## Follow-Up Operations

In stream mode, a stream is created in addition to the database and scratch, and `ms_scan_stream()` is repeatedly called according to the actual input segments. When the stream ends, `ms_flush_stream()` or `ms_close_stream()` is called. For details about API parameters, return values, and usage constraints, see [API Reference](./api_reference.md).
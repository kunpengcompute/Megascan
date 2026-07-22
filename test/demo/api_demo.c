/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Megascan public API call demo entry.
 */
#include "demo_common.h"
#include "benchmark_case_demo.h"
#include "benchmark_runner.h"
#include "data_generator_runner.h"
#include "workflow_runner.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const demo_scenario_t *g_scenarios[] = {
    &g_demo_scenario_runtime_config,
    &g_demo_scenario_legacy_blacklist,
    &g_demo_scenario_legacy_content_scan,
    &g_demo_scenario_security_policy,
    &g_demo_scenario_content_gateway,
    &g_demo_scenario_stream_audit,
    &g_demo_scenario_ioc_lookup,
    &g_demo_scenario_risk_policy
};

typedef enum {
    DEMO_ACTION_NONE,
    DEMO_ACTION_HELP,
    DEMO_ACTION_LIST,
    DEMO_ACTION_APIS,
    DEMO_ACTION_API,
    DEMO_ACTION_MODULES,
    DEMO_ACTION_MODULE,
    DEMO_ACTION_MODULE_TEST,
    DEMO_ACTION_SELFTEST,
    DEMO_ACTION_BENCHMARK_CASE,
    DEMO_ACTION_APP_SCAN,
    DEMO_ACTION_APP_LOOKUP,
    DEMO_ACTION_ALL,
    DEMO_ACTION_SCENARIO,
    DEMO_ACTION_INTERACTIVE
} demo_action_t;

static int equals_ignore_case(const char *lhs, const char *rhs)
{
    while (*lhs != '\0' && *rhs != '\0') {
        if (tolower((unsigned char)*lhs) != tolower((unsigned char)*rhs)) {
            return 0;
        }
        lhs++;
        rhs++;
    }
    return *lhs == '\0' && *rhs == '\0';
}

static void print_usage(const char *prog)
{
    printf("Megascan API 调用示例\n\n");
    printf("用法：%s <操作> [选项]\n\n", prog);
    printf("操作：\n");
    printf("  scan                       扫描文本、文件或 920hg 数据\n");
    printf("  lookup                     执行 HASH 精确查询\n");
    printf("  corpus                     扫描 920hg 单条查询或完整 corpus\n");
    printf("  benchmark                  运行完整 920hg benchmark 核心\n");
    printf("  generate-data              生成 920hg 规则或 corpus 文件\n");
    printf("  920hg                      运行完整 920hg 生成、测试和结果分析流程\n");
    printf("  logical                    运行逻辑策略计算\n");
    printf("  allocator                  运行内存分配器配置\n");
    printf("  apis                       查看可单独执行的接口测试\n");
    printf("  modules                    查看接口模块和流程图\n");
    printf("  selftest                   运行全部单接口测试并汇总\n");
    printf("  scenarios                  查看内置应用场景\n");
    printf("  -h, --help                 显示帮助信息\n");
    printf("  --list                     列出可运行场景\n");
    printf("  --apis                     列出可单独执行的接口测试\n");
    printf("  --api <接口名> [参数...]   按 CLI 参数调用指定接口；无参数时运行自动测试\n");
    printf("  --module <模块名>          显示模块接口流程图\n");
    printf("  --module-test <模块名>     运行模块内全部接口测试\n");
    printf("  --scenario <场景名>        运行指定场景\n");
    printf("  --benchmark-case           运行一条 920hg 数据查询\n");
    printf("  -a, --all                  运行全部场景\n");
    printf("  -i, --interactive          进入交互菜单\n");
    printf("\n输入选项：\n");
    printf("  --input <文本>             扫描指定文本\n");
    printf("  --input-file <路径>        扫描指定文件\n");
    printf("  --pattern <文本>           替换场景中的第一条 AC 规则\n");
    printf("  --key <文本>               指定 HASH 规则及查询值\n");
    printf("  --chunk-size <字节数>      设置自定义流数据的分块大小\n");
    printf("  --expression <表达式>      设置 logical 命令使用的逻辑表达式\n");
    printf("  --set <状态>               设置逻辑状态，例如 101=1,102=0\n");
    printf("\n920hg 数据选项：\n");
    printf("  --algorithm <算法>         hash、ac、accombo、ac_legacy 或 ac_on_combo\n");
    printf("  --mode <模式>              lookup、block 或 stream-full\n");
    printf("  --rules-file <路径>        920hg 规则文件\n");
    printf("  --corpus-file <路径>       920hg corpus 文件\n");
    printf("  --rule-count <数量>        加载的规则数量\n");
    printf("  --query-index <序号>       执行的查询序号，默认 0\n");
    printf("  --all-queries              扫描 corpus 中的全部查询\n");
    printf("\n每个操作和输入选项都支持在后面添加 -h 或 --help 查看详细帮助。\n");
    printf("\n示例：\n");
    printf("  %s --scenario content-gateway --pattern token= --input 'token=abc'\n", prog);
    printf("  %s --scenario stream-audit --input-file request.dat --chunk-size 64\n", prog);
    printf("  %s --scenario ioc-lookup --key malware-sha256-demo\n", prog);
    printf("  %s --api ms_scan_database abcdef 4 ac 0 0 abc 101 0 def 102 0 '101&102' 1001 1\n", prog);
    printf("  %s --api ms_set_scratch_allocator\n", prog);
    printf("  %s --module stream\n", prog);
    printf("  %s --module-test delete\n", prog);
    printf("  %s selftest\n", prog);
    printf("  %s --benchmark-case --algorithm ac --mode block --rules-file rules.tsv ", prog);
    printf("--corpus-file corpus.bin --rule-count 500000 --query-index 0\n");
    printf("  %s --all\n", prog);
    printf("  %s corpus --algorithm ac --rules-file rules.tsv ", prog);
    printf("--corpus-file corpus.bin --rule-count 50000 --all-queries\n");
    printf("\n不带参数运行时，将直接进入交互菜单。\n");
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0 || parsed > UINT32_MAX) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int parse_u32_allow_zero(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int load_input_file(const char *path, uint8_t **data, uint32_t *length)
{
    FILE *fp = fopen(path, "rb");
    long size;
    uint8_t *buffer;

    if (fp == NULL) {
        printf("无法打开输入文件：%s\n", path);
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 ||
        (unsigned long)size > UINT32_MAX || fseek(fp, 0, SEEK_SET) != 0) {
        printf("无法读取输入文件大小：%s\n", path);
        (void)fclose(fp);
        return -1;
    }
    buffer = (uint8_t *)malloc(size == 0 ? 1 : (size_t)size);
    if (buffer == NULL) {
        printf("无法为输入文件分配内存：%s\n", path);
        (void)fclose(fp);
        return -1;
    }
    if (size > 0 && fread(buffer, 1, (size_t)size, fp) != (size_t)size) {
        printf("无法读取输入文件：%s\n", path);
        free(buffer);
        (void)fclose(fp);
        return -1;
    }
    (void)fclose(fp);
    *data = buffer;
    *length = (uint32_t)size;
    return 0;
}

static void list_scenarios(void)
{
    uint32_t i;

    printf("可运行场景：\n");
    for (i = 0; i < DEMO_ARRAY_SIZE(g_scenarios); i++) {
        printf("  %-20s %s\n", g_scenarios[i]->key, g_scenarios[i]->title);
        printf("                       %s\n", g_scenarios[i]->summary);
    }
}

static void list_apis(void)
{
    demo_list_api_tests();
}

static const demo_scenario_t *find_scenario(const char *key)
{
    uint32_t i;

    for (i = 0; i < DEMO_ARRAY_SIZE(g_scenarios); i++) {
        if (equals_ignore_case(g_scenarios[i]->key, key)) {
            return g_scenarios[i];
        }
    }
    return NULL;
}

static int is_help_argument(const char *argument)
{
    return strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0;
}

static void print_scenario_help(const char *prog, const char *key)
{
    const demo_scenario_t *scenario = find_scenario(key);
    uint32_t i;

    if (scenario == NULL) {
        printf("未知场景：%s\n\n", key);
        list_scenarios();
        return;
    }
    printf("场景：%s（%s）\n", scenario->title, scenario->key);
    printf("说明：%s\n", scenario->summary);
    printf("用法：%s --scenario %s [输入选项]\n", prog, scenario->key);
    printf("调用接口：\n");
    for (i = 0; scenario->apis[i] != NULL; i++) {
        printf("  %s\n", scenario->apis[i]);
    }
    printf("\n输入选项可使用 --input、--input-file、--pattern、--key 和 --chunk-size；");
    printf("不适用于该场景的参数会被忽略。\n");
}

static void print_api_help(const char *prog, const char *api)
{
    demo_print_api_test_help(prog, api);
}

static void print_command_help(const char *prog, const char *command)
{
    if (strcmp(command, "scan") == 0) {
        printf("用途：使用 AC 类匹配器扫描文本、普通文件或规则文件。\n\n");
        printf("用法一：%s scan --matcher ac --mode block --pattern <规则> --input <文本>\n", prog);
        printf("用法二：%s scan --matcher <算法> --mode <模式> --rules-file <路径> ", prog);
        printf("--rule-count <数量> --input-file <路径> [--chunk-size <字节数>]\n\n");
        printf("算法：ac、accombo、ac_legacy、ac_on_combo。\n");
        printf("模式：block 或 stream-full；chunk-size 只对 stream-full 生效。\n");
        printf("规则：ac/accombo 使用 920hg 格式的 ID<TAB>flags<TAB>expression TSV 文件。\n\n");
        printf("示例：\n");
        printf("  %s scan --matcher ac --mode block --pattern 'token=' --input 'token=abc'\n", prog);
        printf("  %s scan --matcher accombo --mode stream-full \\\n", prog);
        printf("    --rules-file rules.tsv --rule-count 50000 \\\n");
        printf("    --input-file request.dat --chunk-size 64\n");
    } else if (strcmp(command, "lookup") == 0) {
        printf("用途：执行 HASH 精确查询。\n\n");
        printf("用法一：%s lookup --key <文本>\n", prog);
        printf("用法二：%s lookup --rules-file <二进制规则> --rule-count <数量> ", prog);
        printf("--input-file <16字节tuple>\n");
        printf("用法三：%s lookup --rules-file <二进制规则> --rule-count <数量> ", prog);
        printf("--corpus-file <920hg corpus> [--query-index <序号> | --all-queries]\n\n");
        printf("示例：\n");
        printf("  %s lookup --key malware-sha256-demo\n", prog);
        printf("  %s lookup --rules-file \"$DATA/shared/hash_90000000_rules.bin\" \\\n", prog);
        printf("    --rule-count 5000000 --corpus-file \"$DATA/shared/hash_q200000_h5000.bin\" \\\n");
        printf("    --query-index 0\n");
    } else if (strcmp(command, "corpus") == 0) {
        printf("用途：使用 920hg 原始规则和 corpus 做单条复现或全量正确性扫描。\n\n");
        printf("用法：%s corpus --algorithm <算法> --rules-file <路径> ", prog);
        printf("--corpus-file <路径> --rule-count <数量> ");
        printf("[--mode <模式>] [--query-index <序号> | --all-queries] ");
        printf("[--chunk-size <字节数>]\n\n");
        printf("算法与数据：\n");
        printf("  hash         HASH rules.bin + hash corpus，只支持 lookup\n");
        printf("  ac           普通 AC rules.tsv + AC corpus\n");
        printf("  accombo      ACCombo rules.tsv + ACCombo corpus\n");
        printf("  ac_legacy    普通 AC rules.tsv + AC corpus\n");
        printf("  ac_on_combo  ACCombo rules.tsv + ACCombo corpus，只编译原子规则\n\n");
        printf("模式：HASH 固定为 lookup；其他算法默认为 block，也可使用 stream-full。\n");
        printf("查询：默认执行 query-index=0；all-queries 会编译一次规则并扫描完整 corpus。\n");
        printf("校验：根据 corpus 的 expected_ids/expected_candidates 检查结果；不统计吞吐量。\n\n");
        printf("v4 数据示例（先设置 DATA=/path/to/data/Megascan/920hg_data_v4）：\n");
        printf("  %s corpus --algorithm ac --mode block \\\n", prog);
        printf("    --rules-file \"$DATA/shared/ac_50000_rules.tsv\" --rule-count 50000 \\\n");
        printf("    --corpus-file \"$DATA/shared/ac_q200000_p560_h5000.bin\" --query-index 0\n\n");
        printf("  %s corpus --algorithm accombo --mode stream-full \\\n", prog);
        printf("    --rules-file \"$DATA/shared/accombo_50000_rules.tsv\" --rule-count 50000 \\\n");
        printf("    --corpus-file \"$DATA/shared/accombo_q200000_p560_h5000.bin\" --all-queries\n");
    } else if (strcmp(command, "benchmark") == 0) {
        printf("用途：运行与 test/demo/920hg/bench_920hg.c 完全相同的 benchmark 核心。\n\n");
        printf("用法：%s benchmark -a <算法> -M <模式> -n <规则数> -t <规则文件> ", prog);
        printf("-s <corpus文件> -p <CSV输出> -T <机器名> ");
        printf("[-R <次数>] [-w <预热>] [-c <CPU列表>]\n\n");
        printf("算法：hash、ac、accombo、ac_legacy、ac_on_combo、ac_delete、accombo_delete。\n");
        printf("模式：lookup、block、stream-full、both。\n");
        printf("CPU列表：例如 1、1,3,5 或 1-95；每个 worker 扫描完整 corpus。\n");
        printf("输出：正确性校验、query_ops、GiB/s、编译耗时、RSS 和原始 CSV。\n\n");
        printf("注意：正式性能基线请使用 920hg single 或 920hg full；\n");
        printf("      本命令用于手动调用同一 benchmark 核心，不与基线性能数据混用。\n\n");
        printf("示例（5万 AC、20万 query、块和流各1遍）：\n");
        printf("  %s benchmark -a ac -M both -n 50000 \\\n", prog);
        printf("    -t \"$DATA/shared/ac_50000_rules.tsv\" \\\n");
        printf("    -s \"$DATA/shared/ac_q200000_p560_h5000.bin\" \\\n");
        printf("    -p ./ac_raw.csv -T local -R 1 -w 0 -c 1\n");
    } else if (strcmp(command, "generate-data") == 0) {
        printf("用途：确定性生成一组 920hg 规则文件或 corpus 文件。\n\n");
        printf("用法：%s generate-data -a hash|ac|accombo -n <规则数> ", prog);
        printf("[-c <组合规则数>] [-q <查询数>] [-H <命中基点>] ");
        printf("[-S <seed>] [-L <payload字节>] [-t <规则文件>] [-s <corpus文件>]\n\n");
        printf("命中基点范围为 0..10000，例如 5000 表示 50%%。\n");
        printf("生成 corpus 时必须指定 -q；至少指定 -t 或 -s 之一。\n\n");
        printf("示例：\n");
        printf("  %s generate-data -a ac -n 50000 -q 200000 -H 5000 \\\n", prog);
        printf("    -S 9202026 -L 560 -t ac_rules.tsv -s ac_corpus.bin\n");
    } else if (strcmp(command, "logical") == 0) {
        printf("用法：%s logical [--expression <表达式>] [--set <状态>]\n", prog);
        printf("运行逻辑表达式解析、状态设置和求值流程，例如 --set 101=1,102=0。\n");
    } else if (strcmp(command, "allocator") == 0) {
        printf("用法：%s allocator\n运行自定义 allocator 的设置、使用和恢复流程。\n", prog);
    } else if (strcmp(command, "apis") == 0) {
        printf("用法：%s apis\n查看可以单独运行并带断言的公开接口测试。\n", prog);
    } else if (strcmp(command, "modules") == 0) {
        printf("用法：%s modules\n列出接口模块。\n", prog);
        printf("查看流程：%s --module <模块名>\n", prog);
        printf("运行测试：%s --module-test <模块名>\n", prog);
    } else if (strcmp(command, "selftest") == 0) {
        printf("用法：%s selftest\n运行全部单接口测试并用退出码返回汇总结果。\n", prog);
    } else if (strcmp(command, "scenarios") == 0) {
        printf("用法：%s scenarios\n列出全部内置应用场景。\n", prog);
    } else if (strcmp(command, "--list") == 0) {
        printf("用法：%s --list\n列出所有可运行场景及其用途。\n", prog);
    } else if (strcmp(command, "--apis") == 0) {
        printf("用法：%s --apis\n列出可以单独运行并带断言的公开接口测试。\n", prog);
    } else if (strcmp(command, "--api") == 0) {
        printf("用法一：%s --api <接口名>\n", prog);
        printf("        无参数时运行该接口自动测试。\n");
        printf("用法二：%s --api <接口名> [参数...]\n", prog);
        printf("        有参数时按 CLI 参数调用接口，例如：\n");
        printf("        %s --api ms_compile_database_multi ac 0 0 abc 101 0\n", prog);
        printf("        %s --api ms_scan_database abcdef 4 ac 0 0 abc 101 0 def 102 0 '101&102' 1001 1\n", prog);
    } else if (strcmp(command, "--module") == 0) {
        printf("用法：%s --module <模块名>\n显示模块的接口调用流程和接口清单。\n", prog);
    } else if (strcmp(command, "--module-test") == 0) {
        printf("用法：%s --module-test <模块名>\n显示流程并运行模块内全部单接口测试。\n", prog);
    } else if (strcmp(command, "--scenario") == 0 || strcmp(command, "--case") == 0 ||
               strcmp(command, "-c") == 0) {
        printf("用法：%s --scenario <场景名> [输入选项]\n", prog);
        printf("运行一个完整接口调用场景。\n\n");
        list_scenarios();
    } else if (strcmp(command, "--all") == 0 || strcmp(command, "-a") == 0) {
        printf("用法：%s --all [输入选项]\n依次运行全部接口调用场景。\n", prog);
    } else if (strcmp(command, "--benchmark-case") == 0) {
        printf("用法：%s --benchmark-case --algorithm <算法> --mode <模式> ", prog);
        printf("--rules-file <路径> --corpus-file <路径> --rule-count <数量> ");
        printf("[--query-index <序号>]\n");
        printf("解析 920hg 规则和 corpus，执行一条查询并校验期望 ID 数量，不统计性能。\n");
    } else if (strcmp(command, "--interactive") == 0 || strcmp(command, "-i") == 0) {
        printf("用法：%s --interactive [输入选项]\n进入交互菜单并手动选择场景。\n", prog);
    } else if (strcmp(command, "--input") == 0) {
        printf("用法：%s <操作> --input <文本>\n直接使用命令行文本作为扫描数据。\n", prog);
    } else if (strcmp(command, "--input-file") == 0) {
        printf("用法：%s <操作> --input-file <路径>\n读取文件内容作为扫描数据。\n", prog);
    } else if (strcmp(command, "--pattern") == 0) {
        printf("用法：%s <操作> --pattern <文本>\n替换场景中的第一条 AC 规则。\n", prog);
    } else if (strcmp(command, "--key") == 0) {
        printf("用法：%s <操作> --key <文本>\n设置 HASH 场景加载并查询的 key。\n", prog);
    } else if (strcmp(command, "--chunk-size") == 0) {
        printf("用法：%s <操作> --chunk-size <字节数>\n设置自定义流输入每次扫描的数据大小，必须大于 0。\n", prog);
    } else if (strcmp(command, "--algorithm") == 0) {
        printf("用法：%s --benchmark-case --algorithm <算法>\n", prog);
        printf("支持 hash、ac、accombo、ac_legacy 和 ac_on_combo。\n");
    } else if (strcmp(command, "--mode") == 0) {
        printf("用法：%s --benchmark-case --mode <模式>\n", prog);
        printf("HASH 使用 lookup；其他算法使用 block 或 stream-full。\n");
    } else if (strcmp(command, "--rules-file") == 0) {
        printf("用法：%s --benchmark-case --rules-file <路径>\n指定 920hg 的二进制 HASH 规则或 AC TSV 规则文件。\n", prog);
    } else if (strcmp(command, "--corpus-file") == 0) {
        printf("用法：%s --benchmark-case --corpus-file <路径>\n指定 920hg 生成的二进制 corpus 文件。\n", prog);
    } else if (strcmp(command, "--rule-count") == 0) {
        printf("用法：%s --benchmark-case --rule-count <数量>\n指定从规则文件开头加载的规则数量。\n", prog);
    } else if (strcmp(command, "--query-index") == 0) {
        printf("用法：%s --benchmark-case --query-index <序号>\n指定 corpus 中从 0 开始的查询序号，默认 0。\n", prog);
    } else if (strcmp(command, "--all-queries") == 0) {
        printf("用法：%s corpus [920hg 数据选项] --all-queries\n", prog);
        printf("规则只编译一次，然后扫描 corpus 中的全部查询并输出正确性汇总。\n");
    } else if (strcmp(command, "--matcher") == 0) {
        printf("用法：%s scan --matcher <算法>\n支持 ac、accombo、ac_legacy 和 ac_on_combo。\n", prog);
    } else if (strcmp(command, "--expression") == 0) {
        printf("用法：%s logical --expression <表达式>\n指定需要解析和计算的逻辑表达式。\n", prog);
    } else if (strcmp(command, "--set") == 0) {
        printf("用法：%s logical --set <ID=值,...>\n例如：--set 101=1,102=0。\n", prog);
    } else {
        print_usage(prog);
    }
}

static void run_scenario(const demo_scenario_t *scenario)
{
    const demo_options_t *options = demo_get_options();

    printf("\n-- %s [%s] --\n", scenario->title, scenario->key);
    printf("%s\n", scenario->summary);
    if (options->input != NULL) {
        printf("自定义输入：%u 字节\n", options->input_len);
    }
    if (options->pattern != NULL) {
        printf("第一条 AC 规则：%s\n", options->pattern);
    }
    if (options->key != NULL) {
        printf("HASH 查询 key：%s\n", options->key);
    }
    if (options->chunk_size != 0) {
        printf("流数据块大小：%u 字节\n", options->chunk_size);
    }
    scenario->fn();
}

static void run_all_scenarios(void)
{
    uint32_t i;

    for (i = 0; i < DEMO_ARRAY_SIZE(g_scenarios); i++) {
        run_scenario(g_scenarios[i]);
    }
}

static int run_api_demo(const char *api)
{
    return demo_run_api_test(api);
}

static int parse_menu_choice(const char *line)
{
    char *end = NULL;
    long value;

    value = strtol(line, &end, 10);
    if (end == line) {
        return -1;
    }
    while (*end != '\0') {
        if (!isspace((unsigned char)*end)) {
            return -1;
        }
        end++;
    }
    return (int)value;
}

static void read_menu_value(const char *prompt, char *buffer, size_t capacity)
{
    size_t length;

    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buffer, (int)capacity, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }
}

static void interactive_menu(void)
{
    char line[64];
    char input[1024] = {0};
    char pattern[256] = {0};
    char key[256] = {0};
    char api[128] = {0};
    char module[64] = {0};
    demo_options_t options = *demo_get_options();

    for (;;) {
        uint32_t i;
        int choice;

        printf("\nMegascan API 示例菜单：\n");
        printf("  0. 运行全部场景\n");
        for (i = 0; i < DEMO_ARRAY_SIZE(g_scenarios); i++) {
            printf("  %u. %s (%s)\n", i + 1, g_scenarios[i]->title, g_scenarios[i]->key);
        }
        printf("  h. 查看帮助\n");
        printf("  a. 查看可单独测试的公开接口\n");
        printf("  u. 运行一个单接口测试\n");
        printf("  m. 查看一个模块流程图\n");
        printf("  s. 运行全部单接口测试\n");
        printf("  t. 设置扫描文本\n");
        printf("  p. 设置第一条 AC 规则\n");
        printf("  k. 设置 HASH 查询 key\n");
        printf("  c. 设置流数据块大小\n");
        printf("  x. 清空自定义输入参数\n");
        printf("  q. 退出\n");
        printf("请选择> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            return;
        }
        if (line[0] == 'q' || line[0] == 'Q') {
            return;
        }
        if (line[0] == 'h' || line[0] == 'H') {
            print_usage("megascan_demo");
            continue;
        }
        if (line[0] == 'a' || line[0] == 'A') {
            list_apis();
            continue;
        }
        if (line[0] == 'u' || line[0] == 'U') {
            read_menu_value("输入公开接口名> ", api, sizeof(api));
            (void)demo_run_api_test(api);
            continue;
        }
        if (line[0] == 'm' || line[0] == 'M') {
            demo_list_modules();
            read_menu_value("输入模块名> ", module, sizeof(module));
            (void)demo_show_module(module);
            continue;
        }
        if (line[0] == 's' || line[0] == 'S') {
            (void)demo_run_all_api_tests();
            continue;
        }
        if (line[0] == 't' || line[0] == 'T') {
            read_menu_value("输入扫描文本> ", input, sizeof(input));
            options.input = (const uint8_t *)input;
            options.input_len = (uint32_t)strlen(input);
            demo_set_options(&options);
            continue;
        }
        if (line[0] == 'p' || line[0] == 'P') {
            read_menu_value("输入第一条 AC 规则> ", pattern, sizeof(pattern));
            options.pattern = pattern[0] != '\0' ? pattern : NULL;
            demo_set_options(&options);
            continue;
        }
        if (line[0] == 'k' || line[0] == 'K') {
            read_menu_value("输入 HASH 查询 key> ", key, sizeof(key));
            options.key = key[0] != '\0' ? key : NULL;
            demo_set_options(&options);
            continue;
        }
        if (line[0] == 'c' || line[0] == 'C') {
            read_menu_value("输入流数据块大小> ", line, sizeof(line));
            if (parse_u32(line, &options.chunk_size) != 0) {
                printf("流数据块大小无效。\n");
            } else {
                demo_set_options(&options);
            }
            continue;
        }
        if (line[0] == 'x' || line[0] == 'X') {
            (void)memset(&options, 0, sizeof(options));
            demo_set_options(&options);
            printf("已清空自定义输入参数。\n");
            continue;
        }

        choice = parse_menu_choice(line);
        if (choice == 0) {
            run_all_scenarios();
        } else if (choice > 0 && (uint32_t)choice <= DEMO_ARRAY_SIZE(g_scenarios)) {
            run_scenario(g_scenarios[(uint32_t)choice - 1]);
        } else {
            printf("未知选项。输入 h 查看帮助，输入 a 查看接口，输入 q 退出。\n");
        }
    }
}

int main(int argc, char **argv)
{
    const char *prog = argc > 0 ? argv[0] : "megascan_demo";
    demo_options_t options = {0};
    demo_benchmark_case_t benchmark_case = {0};
    demo_action_t action = DEMO_ACTION_NONE;
    const char *action_value = NULL;
    const char *input_file = NULL;
    uint8_t *owned_input = NULL;
    int i;
    int ret = 0;

    if (argc == 1) {
        interactive_menu();
        return 0;
    }
    if (strcmp(argv[1], "benchmark") == 0) {
        for (i = 2; i < argc; i++) {
            if (is_help_argument(argv[i])) {
                print_command_help(prog, "benchmark");
                return 0;
            }
        }
        return demo_run_920hg_benchmark(argc - 1, argv + 1);
    }
    if (strcmp(argv[1], "generate-data") == 0) {
        for (i = 2; i < argc; i++) {
            if (is_help_argument(argv[i])) {
                print_command_help(prog, "generate-data");
                return 0;
            }
        }
        return demo_run_920hg_data_generator(argc - 1, argv + 1);
    }
    if (strcmp(argv[1], "920hg") == 0 || strcmp(argv[1], "--920hg") == 0) {
        return demo_run_920hg_workflow(argc - 1, argv + 1);
    }
    if (strcmp(argv[1], "--api") == 0) {
        if (argc < 3) {
            demo_print_api_cli_help(prog);
            return 1;
        }
        if (argc == 3 && is_help_argument(argv[2])) {
            demo_print_api_cli_help(prog);
            return 0;
        }
        if (argc == 4 && is_help_argument(argv[3])) {
            print_api_help(prog, argv[2]);
            return 0;
        }
        return demo_run_api_cli(argv[2], argc - 3, argv + 3);
    }
    if (argc == 3 && is_help_argument(argv[2])) {
        print_command_help(prog, argv[1]);
        return 0;
    }
    if (argc == 4 && is_help_argument(argv[3])) {
        if (strcmp(argv[1], "--scenario") == 0 || strcmp(argv[1], "--case") == 0 ||
            strcmp(argv[1], "-c") == 0) {
            print_scenario_help(prog, argv[2]);
            return 0;
        }
        if (strcmp(argv[1], "--api") == 0) {
            print_api_help(prog, argv[2]);
            return 0;
        }
        if (strcmp(argv[1], "--module") == 0 || strcmp(argv[1], "--module-test") == 0) {
            (void)demo_show_module(argv[2]);
            return 0;
        }
    }
    for (i = 1; i + 1 < argc; i++) {
        if (is_help_argument(argv[i + 1])) {
            print_command_help(prog, argv[i]);
            return 0;
        }
    }

    for (i = 1; i < argc; i++) {
        demo_action_t next_action = DEMO_ACTION_NONE;

        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            next_action = DEMO_ACTION_HELP;
        } else if (strcmp(argv[i], "--list") == 0) {
            next_action = DEMO_ACTION_LIST;
        } else if (strcmp(argv[i], "--apis") == 0) {
            next_action = DEMO_ACTION_APIS;
        } else if (strcmp(argv[i], "modules") == 0) {
            next_action = DEMO_ACTION_MODULES;
        } else if (strcmp(argv[i], "selftest") == 0) {
            next_action = DEMO_ACTION_SELFTEST;
        } else if (strcmp(argv[i], "--all") == 0 || strcmp(argv[i], "-a") == 0) {
            next_action = DEMO_ACTION_ALL;
        } else if (strcmp(argv[i], "--benchmark-case") == 0) {
            next_action = DEMO_ACTION_BENCHMARK_CASE;
        } else if (strcmp(argv[i], "corpus") == 0) {
            next_action = DEMO_ACTION_BENCHMARK_CASE;
        } else if (strcmp(argv[i], "scan") == 0) {
            next_action = DEMO_ACTION_APP_SCAN;
        } else if (strcmp(argv[i], "lookup") == 0) {
            next_action = DEMO_ACTION_APP_LOOKUP;
        } else if (strcmp(argv[i], "logical") == 0) {
            next_action = DEMO_ACTION_SCENARIO;
            action_value = "risk-policy";
        } else if (strcmp(argv[i], "allocator") == 0) {
            next_action = DEMO_ACTION_SCENARIO;
            action_value = "runtime-config";
        } else if (strcmp(argv[i], "apis") == 0) {
            next_action = DEMO_ACTION_APIS;
        } else if (strcmp(argv[i], "scenarios") == 0) {
            next_action = DEMO_ACTION_LIST;
        } else if (strcmp(argv[i], "--interactive") == 0 || strcmp(argv[i], "-i") == 0) {
            next_action = DEMO_ACTION_INTERACTIVE;
        } else if (strcmp(argv[i], "--api") == 0 || strcmp(argv[i], "--module") == 0 ||
                   strcmp(argv[i], "--module-test") == 0 || strcmp(argv[i], "--scenario") == 0 ||
                   strcmp(argv[i], "--case") == 0 || strcmp(argv[i], "-c") == 0) {
            if (++i >= argc) {
                printf("操作缺少参数。\n\n");
                print_usage(prog);
                return 1;
            }
            if (strcmp(argv[i - 1], "--api") == 0) {
                next_action = DEMO_ACTION_API;
            } else if (strcmp(argv[i - 1], "--module") == 0) {
                next_action = DEMO_ACTION_MODULE;
            } else if (strcmp(argv[i - 1], "--module-test") == 0) {
                next_action = DEMO_ACTION_MODULE_TEST;
            } else {
                next_action = DEMO_ACTION_SCENARIO;
            }
            action_value = argv[i];
        } else if (strcmp(argv[i], "--all-queries") == 0) {
            benchmark_case.all_queries = 1;
            continue;
        } else if (strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "--input-file") == 0 ||
                   strcmp(argv[i], "--pattern") == 0 || strcmp(argv[i], "--key") == 0 ||
                   strcmp(argv[i], "--expression") == 0 || strcmp(argv[i], "--set") == 0 ||
                   strcmp(argv[i], "--chunk-size") == 0 ||
                   strcmp(argv[i], "--algorithm") == 0 || strcmp(argv[i], "--matcher") == 0 ||
                   strcmp(argv[i], "--mode") == 0 ||
                   strcmp(argv[i], "--rules-file") == 0 || strcmp(argv[i], "--corpus-file") == 0 ||
                   strcmp(argv[i], "--rule-count") == 0 || strcmp(argv[i], "--query-index") == 0) {
            const char *option = argv[i];

            if (++i >= argc) {
                printf("%s 缺少参数。\n", option);
                return 1;
            }
            if (strcmp(option, "--input") == 0) {
                options.input = (const uint8_t *)argv[i];
                options.input_len = (uint32_t)strlen(argv[i]);
            } else if (strcmp(option, "--input-file") == 0) {
                input_file = argv[i];
            } else if (strcmp(option, "--pattern") == 0) {
                options.pattern = argv[i];
            } else if (strcmp(option, "--key") == 0) {
                options.key = argv[i];
            } else if (strcmp(option, "--expression") == 0) {
                options.logical_expression = argv[i];
            } else if (strcmp(option, "--set") == 0) {
                options.logical_values = argv[i];
            } else if (strcmp(option, "--algorithm") == 0 || strcmp(option, "--matcher") == 0) {
                benchmark_case.algorithm = argv[i];
            } else if (strcmp(option, "--mode") == 0) {
                benchmark_case.mode = argv[i];
            } else if (strcmp(option, "--rules-file") == 0) {
                benchmark_case.rules_file = argv[i];
            } else if (strcmp(option, "--corpus-file") == 0) {
                benchmark_case.corpus_file = argv[i];
            } else if (strcmp(option, "--rule-count") == 0) {
                if (parse_u32(argv[i], &benchmark_case.rule_count) != 0) {
                    printf("规则数量无效：%s\n", argv[i]);
                    return 1;
                }
            } else if (strcmp(option, "--query-index") == 0) {
                if (parse_u32_allow_zero(argv[i], &benchmark_case.query_index) != 0) {
                    printf("查询序号无效：%s\n", argv[i]);
                    return 1;
                }
            } else if (parse_u32(argv[i], &options.chunk_size) != 0) {
                printf("流数据块大小无效：%s\n", argv[i]);
                return 1;
            }
            continue;
        } else {
            printf("未知选项：%s\n\n", argv[i]);
            print_usage(prog);
            return 1;
        }

        if (action != DEMO_ACTION_NONE) {
            printf("一次只能指定一个操作。\n\n");
            print_usage(prog);
            return 1;
        }
        action = next_action;
    }

    if (input_file != NULL && options.input != NULL) {
        printf("--input 和 --input-file 不能同时使用。\n");
        return 1;
    }
    if (input_file != NULL && load_input_file(input_file, &owned_input, &options.input_len) != 0) {
        return 1;
    }
    if (owned_input != NULL) {
        options.input = owned_input;
    }
    benchmark_case.chunk_size = options.chunk_size;
    demo_set_options(&options);

    if (action == DEMO_ACTION_HELP) {
        print_usage(prog);
    } else if (action == DEMO_ACTION_LIST) {
        list_scenarios();
    } else if (action == DEMO_ACTION_APIS) {
        list_apis();
    } else if (action == DEMO_ACTION_API) {
        ret = run_api_demo(action_value);
    } else if (action == DEMO_ACTION_MODULES) {
        demo_list_modules();
    } else if (action == DEMO_ACTION_MODULE) {
        ret = demo_show_module(action_value);
    } else if (action == DEMO_ACTION_MODULE_TEST) {
        ret = demo_run_module_tests(action_value);
    } else if (action == DEMO_ACTION_SELFTEST) {
        ret = demo_run_all_api_tests();
    } else if (action == DEMO_ACTION_ALL) {
        run_all_scenarios();
    } else if (action == DEMO_ACTION_BENCHMARK_CASE) {
        if (benchmark_case.algorithm == NULL || benchmark_case.rules_file == NULL ||
            benchmark_case.corpus_file == NULL || benchmark_case.rule_count == 0) {
            printf("920hg 数据场景缺少必需参数。\n\n");
            print_command_help(prog, "--benchmark-case");
            ret = 1;
        } else {
            if (benchmark_case.mode == NULL) {
                benchmark_case.mode = strcmp(benchmark_case.algorithm, "hash") == 0 ?
                                      "lookup" : "block";
            }
            ret = demo_run_benchmark_case(&benchmark_case);
        }
    } else if (action == DEMO_ACTION_APP_LOOKUP) {
        if (benchmark_case.rules_file != NULL && benchmark_case.corpus_file == NULL &&
            options.input != NULL) {
            benchmark_case.algorithm = "hash";
            benchmark_case.mode = "lookup";
            ret = demo_run_rule_file_scan(&benchmark_case,
                                          options.input, options.input_len);
        } else if (benchmark_case.rules_file != NULL || benchmark_case.corpus_file != NULL) {
            benchmark_case.algorithm = "hash";
            benchmark_case.mode = "lookup";
            if (benchmark_case.rules_file == NULL || benchmark_case.corpus_file == NULL ||
                benchmark_case.rule_count == 0) {
                printf("920hg HASH 查询缺少规则文件、corpus 文件或规则数量。\n");
                ret = 1;
            } else {
                ret = demo_run_benchmark_case(&benchmark_case);
            }
        } else {
            run_scenario(&g_demo_scenario_ioc_lookup);
        }
    } else if (action == DEMO_ACTION_APP_SCAN) {
        const char *matcher = benchmark_case.algorithm != NULL ? benchmark_case.algorithm : "ac";
        const char *mode = benchmark_case.mode != NULL ? benchmark_case.mode : "block";

        if (benchmark_case.rules_file != NULL && benchmark_case.corpus_file == NULL &&
            options.input != NULL) {
            benchmark_case.algorithm = matcher;
            benchmark_case.mode = mode;
            ret = demo_run_rule_file_scan(&benchmark_case,
                                          options.input, options.input_len);
        } else if (benchmark_case.rules_file != NULL || benchmark_case.corpus_file != NULL) {
            benchmark_case.algorithm = matcher;
            benchmark_case.mode = mode;
            if (benchmark_case.rules_file == NULL || benchmark_case.corpus_file == NULL ||
                benchmark_case.rule_count == 0) {
                printf("920hg 扫描缺少规则文件、corpus 文件或规则数量。\n");
                ret = 1;
            } else {
                ret = demo_run_benchmark_case(&benchmark_case);
            }
        } else if (strcmp(matcher, "ac") == 0 && strcmp(mode, "block") == 0) {
            run_scenario(&g_demo_scenario_content_gateway);
        } else if (strcmp(matcher, "ac") == 0 && strcmp(mode, "stream-full") == 0) {
            run_scenario(&g_demo_scenario_stream_audit);
        } else if (strcmp(matcher, "ac_legacy") == 0 && strcmp(mode, "block") == 0) {
            run_scenario(&g_demo_scenario_legacy_content_scan);
        } else {
            printf("该扫描组合需要通过 --rules-file 和 --corpus-file 提供规则与数据。\n");
            ret = 1;
        }
    } else if (action == DEMO_ACTION_INTERACTIVE) {
        interactive_menu();
    } else if (action == DEMO_ACTION_SCENARIO) {
        const demo_scenario_t *scenario = find_scenario(action_value);

        if (scenario == NULL) {
            printf("未知场景：%s\n\n", action_value);
            list_scenarios();
            ret = 1;
        } else {
            run_scenario(scenario);
        }
    } else {
        print_usage(prog);
        ret = 1;
    }

    free(owned_input);
    return ret;
}

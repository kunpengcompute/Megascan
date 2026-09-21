/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Embed the canonical 920hg benchmark in the demo executable.
 */
#include "benchmark_runner.h"

#define main demo_embedded_920hg_main
#include "920hg/bench_920hg.c"
#undef main

int demo_run_920hg_benchmark(int argc, char **argv)
{
    return demo_embedded_920hg_main(argc, argv);
}

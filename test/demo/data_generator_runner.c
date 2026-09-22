/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Embed the canonical 920hg data generator in the demo executable.
 */
#include "data_generator_runner.h"

#define main demo_embedded_920hg_generator_main
#include "920hg/gen_920hg_data.c"
#undef main

int demo_run_920hg_data_generator(int argc, char **argv)
{
    return demo_embedded_920hg_generator_main(argc, argv);
}

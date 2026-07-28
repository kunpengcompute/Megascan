#ifndef MEGASCAN_TEST_DEMO_WORKFLOW_RESULTS_H
#define MEGASCAN_TEST_DEMO_WORKFLOW_RESULTS_H

#include <string>

int demo_920hg_summarize(const std::string &out_dir, double max_difference = 0.10);
int demo_920hg_compare(const std::string &left_summary,
                       const std::string &right_summary,
                       const std::string &left_env,
                       const std::string &right_env,
                       const std::string &output, double min_speedup = 1.30);

#endif

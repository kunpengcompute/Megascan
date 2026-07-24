/* Native CSV summarization for the 920hg workflow. */
#include "workflow_results.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
using Row = std::map<std::string, std::string>;

static std::vector<std::string> parse_csv_line(const std::string &line)
{
    std::vector<std::string> fields;
    std::string value;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (quoted && c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
            value += '"';
            i++;
        } else if (c == '"') {
            quoted = !quoted;
        } else if (c == ',' && !quoted) {
            fields.push_back(value);
            value.clear();
        } else {
            value += c;
        }
    }
    fields.push_back(value);
    return fields;
}

static std::vector<Row> read_csv(const std::string &path)
{
    std::ifstream input(path);
    std::vector<Row> rows;
    std::string line;
    if (!std::getline(input, line)) return rows;
    std::vector<std::string> header = parse_csv_line(line);
    while (std::getline(input, line)) {
        std::vector<std::string> values = parse_csv_line(line);
        if (values.size() != header.size()) continue;
        Row row;
        for (size_t i = 0; i < header.size(); i++) row[header[i]] = values[i];
        rows.push_back(row);
    }
    return rows;
}

static std::string quoted(const std::string &value)
{
    if (value.find_first_of(",\"\n") == std::string::npos) return value;
    std::string result = "\"";
    for (char c : value) result += c == '"' ? "\"\"" : std::string(1, c);
    return result + "\"";
}

static int write_csv(const std::string &path, const std::vector<std::string> &fields,
                     const std::vector<Row> &rows)
{
    std::ofstream output(path);
    if (!output) return 1;
    for (size_t i = 0; i < fields.size(); i++) output << (i ? "," : "") << fields[i];
    output << '\n';
    for (const Row &row : rows) {
        for (size_t i = 0; i < fields.size(); i++) {
            auto found = row.find(fields[i]);
            output << (i ? "," : "") << quoted(found == row.end() ? "" : found->second);
        }
        output << '\n';
    }
    return 0;
}

static std::string key_of(const Row &row, const std::vector<std::string> &fields)
{
    std::string key;
    for (const std::string &field : fields) key += row.at(field) + '\x1f';
    return key;
}

static double number(const Row &row, const std::string &field)
{
    return std::strtod(row.at(field).c_str(), nullptr);
}

static std::string fixed(double value, int precision)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

static double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    size_t middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}

static double percentile(std::vector<double> values, double quantile)
{
    std::sort(values.begin(), values.end());
    if (values.empty()) return 0.0;
    double position = quantile * (values.size() - 1u);
    size_t lower = static_cast<size_t>(position);
    size_t upper = std::min(lower + 1u, values.size() - 1u);
    double fraction = position - lower;
    return values[lower] + (values[upper] - values[lower]) * fraction;
}

static std::map<std::string, std::string> read_env(const std::string &path)
{
    std::ifstream input(path);
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        size_t separator = line.find('=');
        if (separator != std::string::npos && separator > 0)
            values.emplace(line.substr(0, separator), line.substr(separator + 1));
    }
    return values;
}

static std::vector<Row> pair_algorithms(const std::vector<Row> &summary,
                                        const std::string &left_name,
                                        const std::string &right_name,
                                        const std::vector<std::string> &fields,
                                        const std::string &left_prefix,
                                        const std::string &right_prefix)
{
    const std::vector<std::string> key_fields = {"machine", "mode", "rule_count",
        "query_count", "payload_size", "configured_hit_rate"};
    std::map<std::string, std::map<std::string, Row>> groups;
    for (const Row &row : summary) {
        if (row.at("algorithm") == left_name || row.at("algorithm") == right_name)
            groups[key_of(row, key_fields)][row.at("algorithm")] = row;
    }
    std::vector<Row> output;
    for (const auto &group : groups) {
        if (!group.second.count(left_name) || !group.second.count(right_name)) continue;
        const Row &left = group.second.at(left_name);
        const Row &right = group.second.at(right_name);
        Row row;
        for (const std::string &field : key_fields) row[field] = left.at(field);
        double lops = number(left, "query_ops_p50"), rops = number(right, "query_ops_p50");
        double lgib = number(left, "throughput_gib_s_p50"), rgib = number(right, "throughput_gib_s_p50");
        double lcompile = number(left, "compile_ns_p50"), rcompile = number(right, "compile_ns_p50");
        row[left_prefix + "_query_ops_p50"] = fixed(lops, 2);
        row[right_prefix + "_query_ops_p50"] = fixed(rops, 2);
        row[left_prefix + "_over_" + right_prefix + "_query_ops"] = fixed(lops / rops, 6);
        row[left_prefix + "_query_ops_change_pct"] = fixed((lops / rops - 1.0) * 100.0, 3);
        row[left_prefix + "_throughput_gib_s_p50"] = fixed(lgib, 8);
        row[right_prefix + "_throughput_gib_s_p50"] = fixed(rgib, 8);
        row[left_prefix + "_over_" + right_prefix + "_gib_s"] = fixed(lgib / rgib, 6);
        row[left_prefix + "_gib_s_change_pct"] = fixed((lgib / rgib - 1.0) * 100.0, 3);
        row[left_prefix + "_compile_ns_p50"] = fixed(lcompile, 0);
        row[right_prefix + "_compile_ns_p50"] = fixed(rcompile, 0);
        row[left_prefix + "_over_" + right_prefix + "_compile_ns"] = fixed(lcompile / rcompile, 6);
        if (left_name == "accombo") {
            row["accombo_ids_per_query"] = left.at("ids_per_query");
            row["ac_on_combo_ids_per_query"] = right.at("ids_per_query");
            row["accombo_candidates_per_query"] = left.at("configured_candidates_per_query");
            row["ac_on_combo_candidates_per_query"] = right.at("configured_candidates_per_query");
        }
        output.push_back(row);
    }
    (void)fields;
    return output;
}

static int summarize_dynamic_delete(const std::string &out_dir, bool &failed)
{
    std::vector<Row> raw = read_csv(out_dir + "/raw_delete_trials.csv");
    const std::vector<std::string> group_fields = {
        "machine", "algorithm", "rule_count", "atomic_rule_count",
        "combination_rule_count", "workers", "cpu_list"};
    const std::vector<std::string> fields = {
        "machine", "algorithm", "rule_count", "atomic_rule_count",
        "combination_rule_count", "workers", "cpu_list", "trials",
        "delete_ns_min", "delete_ns_p50", "delete_ns_p95", "delete_ns_max",
        "rules_per_second_p50", "correctness"};
    std::map<std::string, std::vector<Row>> groups;
    std::vector<Row> summary;

    for (const Row &row : raw) {
        groups[key_of(row, group_fields)].push_back(row);
    }
    for (const auto &item : groups) {
        const Row &first = item.second.front();
        Row row;
        std::vector<double> latency;
        std::vector<double> rate;
        bool correct = true;

        for (const std::string &field : group_fields) {
            row[field] = first.at(field);
        }
        for (const Row &trial : item.second) {
            latency.push_back(number(trial, "delete_ns"));
            rate.push_back(number(trial, "rules_per_second"));
            correct &= trial.at("correctness") == "PASS";
        }
        row["trials"] = std::to_string(item.second.size());
        row["delete_ns_min"] =
            fixed(*std::min_element(latency.begin(), latency.end()), 0);
        row["delete_ns_p50"] = fixed(median(latency), 0);
        row["delete_ns_p95"] = fixed(percentile(latency, 0.95), 0);
        row["delete_ns_max"] =
            fixed(*std::max_element(latency.begin(), latency.end()), 0);
        row["rules_per_second_p50"] = fixed(median(rate), 8);
        row["correctness"] = correct ? "PASS" : "FAIL";
        failed |= !correct;
        summary.push_back(row);
    }
    return write_csv(out_dir + "/dynamic_delete_summary.csv", fields, summary);
}
} // namespace

int demo_920hg_summarize(const std::string &out_dir, double max_difference)
{
    std::vector<Row> raw = read_csv(out_dir + "/raw_trials.csv");
    if (raw.empty()) {
        std::cerr << "原始 CSV 没有数据。\n";
        return 1;
    }
    const std::vector<std::string> group_fields = {"machine", "algorithm", "mode",
        "rule_count", "atomic_rule_count", "combination_rule_count", "query_count",
        "payload_size", "configured_hit_rate"};
    const std::vector<std::string> summary_fields = {"machine", "algorithm", "mode",
        "rule_count", "atomic_rule_count", "combination_rule_count", "query_count",
        "payload_size", "configured_hit_rate", "workers", "cpu_list", "trials",
        "query_ops_p50", "throughput_gib_s_p50", "elapsed_ns_p50", "compile_ns_p50",
        "rss_compile_growth_kb_p50", "observed_hit_rate", "ids_per_query",
        "configured_candidates_per_query", "correctness"};
    std::map<std::string, std::vector<Row>> groups;
    for (const Row &row : raw) groups[key_of(row, group_fields)].push_back(row);
    std::vector<Row> summary;
    bool failed = false;
    for (const auto &item : groups) {
        const Row &first = item.second.front();
        Row row;
        for (const std::string &field : group_fields) row[field] = first.at(field);
        row["workers"] = first.at("workers");
        row["cpu_list"] = first.at("cpu_list");
        row["trials"] = std::to_string(item.second.size());
        bool correct = true;
        std::vector<double> ops, gib, elapsed, compile, rss;
        for (const Row &trial : item.second) {
            ops.push_back(number(trial, "query_ops"));
            gib.push_back(number(trial, "throughput_gib_s"));
            elapsed.push_back(number(trial, "elapsed_ns"));
            compile.push_back(number(trial, "compile_ns"));
            rss.push_back(number(trial, "rss_compile_growth_kb"));
            correct &= trial.at("actual_matches") == trial.at("expected_hits") &&
                       trial.at("reported_ids") == trial.at("expected_ids");
        }
        double queries = number(first, "query_count");
        row["query_ops_p50"] = fixed(median(ops), 2);
        row["throughput_gib_s_p50"] = fixed(median(gib), 8);
        row["elapsed_ns_p50"] = fixed(median(elapsed), 0);
        row["compile_ns_p50"] = fixed(median(compile), 0);
        row["rss_compile_growth_kb_p50"] = fixed(median(rss), 0);
        row["observed_hit_rate"] = fixed(number(first, "actual_matches") / queries, 8);
        row["ids_per_query"] = fixed(number(first, "reported_ids") / queries, 8);
        row["configured_candidates_per_query"] = fixed(number(first, "expected_candidates") / queries, 8);
        row["correctness"] = correct ? "PASS" : "FAIL";
        failed |= !correct;
        summary.push_back(row);
    }
    write_csv(out_dir + "/summary.csv", summary_fields, summary);
    if (summarize_dynamic_delete(out_dir, failed) != 0) {
        return 1;
    }

    const std::vector<std::string> mode_key = {"machine", "algorithm", "rule_count",
        "atomic_rule_count", "combination_rule_count", "query_count", "payload_size",
        "configured_hit_rate"};
    std::map<std::string, std::map<std::string, Row>> modes;
    for (const Row &row : summary) if (row.at("mode") == "block" || row.at("mode") == "stream-full")
        modes[key_of(row, mode_key)][row.at("mode")] = row;
    std::vector<Row> mode_rows, gate_rows;
    for (const auto &item : modes) {
        if (!item.second.count("block") || !item.second.count("stream-full")) { failed = true; continue; }
        const Row &block = item.second.at("block"), &stream = item.second.at("stream-full");
        Row row;
        for (const std::string &field : mode_key) row[field] = block.at(field);
        double bops = number(block, "query_ops_p50"), sops = number(stream, "query_ops_p50");
        double bgib = number(block, "throughput_gib_s_p50"), sgib = number(stream, "throughput_gib_s_p50");
        double ratio = sops / bops, difference = std::fabs(ratio - 1.0);
        row["block_query_ops_p50"] = fixed(bops, 2);
        row["stream_query_ops_p50"] = fixed(sops, 2);
        row["stream_over_block_query_ops"] = fixed(ratio, 6);
        row["block_throughput_gib_s_p50"] = fixed(bgib, 8);
        row["stream_throughput_gib_s_p50"] = fixed(sgib, 8);
        row["stream_over_block_gib_s"] = fixed(sgib / bgib, 6);
        row["absolute_difference"] = fixed(difference, 6);
        row["limit"] = fixed(max_difference, 6);
        row["status"] = difference <= max_difference ? "PASS" : "FAIL";
        failed |= row["status"] == "FAIL";
        mode_rows.push_back(row);
        Row gate = row;
        gate.erase("atomic_rule_count"); gate.erase("combination_rule_count");
        gate["stream_over_block"] = gate["stream_over_block_query_ops"];
        gate_rows.push_back(gate);
    }
    const std::vector<std::string> mode_fields = {"machine", "algorithm", "rule_count",
        "atomic_rule_count", "combination_rule_count", "query_count", "payload_size",
        "configured_hit_rate", "block_query_ops_p50", "stream_query_ops_p50",
        "stream_over_block_query_ops", "block_throughput_gib_s_p50",
        "stream_throughput_gib_s_p50", "stream_over_block_gib_s",
        "absolute_difference", "limit", "status"};
    write_csv(out_dir + "/block_vs_stream.csv", mode_fields, mode_rows);
    const std::vector<std::string> gate_fields = {"machine", "algorithm", "rule_count",
        "query_count", "payload_size", "configured_hit_rate", "block_query_ops_p50",
        "stream_query_ops_p50", "stream_over_block", "absolute_difference", "limit", "status"};
    write_csv(out_dir + "/block_stream_gate.csv", gate_fields, gate_rows);

    const std::vector<std::string> pair_base = {"machine", "mode", "rule_count",
        "query_count", "payload_size", "configured_hit_rate"};
    auto pair_fields = [&](const std::string &left, const std::string &right) {
        std::vector<std::string> fields = pair_base;
        for (const std::string suffix : {"query_ops_p50", "throughput_gib_s_p50", "compile_ns_p50"}) {
            fields.push_back(left + "_" + suffix);
            fields.push_back(right + "_" + suffix);
            std::string metric = suffix == "query_ops_p50" ? "query_ops" :
                                 suffix == "throughput_gib_s_p50" ? "gib_s" : "compile_ns";
            fields.push_back(left + "_over_" + right + "_" + metric);
            if (metric != "compile_ns") fields.push_back(left + "_" + metric + "_change_pct");
        }
        return fields;
    };
    std::vector<Row> legacy = pair_algorithms(summary, "ac", "ac_legacy", {}, "ac", "ac_legacy");
    write_csv(out_dir + "/ac_vs_ac_legacy.csv", pair_fields("ac", "ac_legacy"), legacy);
    std::vector<Row> ac_compare = pair_algorithms(summary, "accombo", "ac", {}, "accombo", "ac");
    const std::vector<std::string> ac_compare_fields = {"machine", "mode", "rule_count",
        "query_count", "payload_size", "configured_hit_rate", "ac_query_ops_p50",
        "accombo_query_ops_p50", "accombo_over_ac_query_ops",
        "ac_throughput_gib_s_p50", "accombo_throughput_gib_s_p50",
        "accombo_over_ac_gib_s", "ac_compile_ns_p50", "accombo_compile_ns_p50",
        "accombo_over_ac_compile_ns"};
    write_csv(out_dir + "/accombo_vs_ac.csv", ac_compare_fields, ac_compare);
    std::vector<Row> combo = pair_algorithms(summary, "accombo", "ac_on_combo", {}, "accombo", "ac_on_combo");
    std::vector<std::string> combo_fields = pair_fields("accombo", "ac_on_combo");
    combo_fields.insert(combo_fields.end(), {"accombo_ids_per_query", "ac_on_combo_ids_per_query",
        "accombo_candidates_per_query", "ac_on_combo_candidates_per_query"});
    write_csv(out_dir + "/accombo_vs_ac_on_combo.csv", combo_fields, combo);
    write_csv(out_dir + "/accombo_vs_ac_same_data.csv", combo_fields, combo);

    std::cout << "结果整理完成：行数=" << raw.size() << " 分组=" << summary.size() << '\n';
    return failed ? 2 : 0;
}

int demo_920hg_compare(const std::string &left_path, const std::string &right_path,
                       const std::string &left_env_path, const std::string &right_env_path,
                       const std::string &output, double min_speedup)
{
    const std::vector<std::string> keys = {"algorithm", "mode", "rule_count",
        "atomic_rule_count", "combination_rule_count", "query_count", "payload_size",
        "configured_hit_rate"};
    std::vector<Row> left_rows = read_csv(left_path), right_rows = read_csv(right_path);
    std::map<std::string, Row> left, right;
    for (const Row &row : left_rows) left[key_of(row, keys)] = row;
    for (const Row &row : right_rows) right[key_of(row, keys)] = row;
    const std::vector<std::string> fields = {"algorithm", "mode", "rule_count",
        "atomic_rule_count", "combination_rule_count", "query_count", "payload_size",
        "configured_hit_rate", "correctness_920", "correctness_hg", "query_ops_920",
        "query_ops_hg", "query_ops_920_over_hg", "gib_s_920", "gib_s_hg",
        "gib_s_920_over_hg", "compile_ms_920", "compile_ms_hg",
        "compile_speed_920_over_hg", "min_920_over_hg", "status"};
    std::vector<Row> output_rows;
    bool failed = left.empty() || right.empty() || left.size() != right.size();
    std::map<std::string, std::string> left_env = read_env(left_env_path);
    std::map<std::string, std::string> right_env = read_env(right_env_path);
    const std::vector<std::string> fingerprints = {"MEGASCAN_COMMIT", "SOURCE_DIRTY",
        "ms_scratch_source_sha256", "ms_stream_source_sha256", "ms_acdb_source_sha256",
        "ms_database_source_sha256",
        "bench_source_sha256", "driver_source_sha256", "summarize_source_sha256",
        "manifest_sha256"};
    for (const std::string &field : fingerprints) {
        if (!left_env.count(field) || !right_env.count(field) ||
            left_env[field] != right_env[field]) {
            std::cerr << "环境指纹不一致或缺失：" << field << '\n';
            failed = true;
        }
    }
    if (left_env["SOURCE_DIRTY"] != "0" || right_env["SOURCE_DIRTY"] != "0") failed = true;
    for (const auto &item : left) {
        if (!right.count(item.first)) { failed = true; continue; }
        const Row &l = item.second, &r = right.at(item.first);
        Row row;
        for (const std::string &field : keys) row[field] = l.at(field);
        double lops = number(l, "query_ops_p50"), rops = number(r, "query_ops_p50");
        double lgib = number(l, "throughput_gib_s_p50"), rgib = number(r, "throughput_gib_s_p50");
        double lcompile = number(l, "compile_ns_p50") / 1e6;
        double rcompile = number(r, "compile_ns_p50") / 1e6;
        row["correctness_920"] = l.at("correctness"); row["correctness_hg"] = r.at("correctness");
        row["query_ops_920"] = fixed(lops, 2); row["query_ops_hg"] = fixed(rops, 2);
        row["query_ops_920_over_hg"] = fixed(lops / rops, 6);
        row["gib_s_920"] = fixed(lgib, 8); row["gib_s_hg"] = fixed(rgib, 8);
        row["gib_s_920_over_hg"] = fixed(lgib / rgib, 6);
        row["compile_ms_920"] = fixed(lcompile, 3); row["compile_ms_hg"] = fixed(rcompile, 3);
        row["compile_speed_920_over_hg"] = fixed(rcompile / lcompile, 6);
        row["min_920_over_hg"] = fixed(min_speedup, 6);
        bool pass = l.at("correctness") == "PASS" && r.at("correctness") == "PASS" && lops / rops >= min_speedup;
        row["status"] = pass ? "PASS" : "FAIL";
        failed |= !pass;
        output_rows.push_back(row);
    }
    write_csv(output, fields, output_rows);
    std::cout << "对比完成：" << output << '\n';
    return failed ? 2 : 0;
}

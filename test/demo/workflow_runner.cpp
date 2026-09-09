/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * Description: Native 920hg generation and benchmark workflow.
 */
#include "workflow_runner.h"
#include "data_generator_runner.h"
#include "workflow_results.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

namespace {

const char *RAW_HEADER =
    "machine,algorithm,mode,trial,rule_count,atomic_rule_count,"
    "combination_rule_count,query_count,payload_size,configured_hit_rate,"
    "observed_hit_rate,expected_hits,actual_matches,expected_ids,reported_ids,"
    "expected_candidates,total_bytes,elapsed_ns,query_ops,throughput_gib_s,"
    "compile_ns,rss_compile_growth_kb,warmups,timing_scope,workers,cpu_list";

const char *RAW_DELETE_HEADER =
    "machine,algorithm,trial,rule_count,atomic_rule_count,"
    "combination_rule_count,target_kind,locator,removed_alias,"
    "removed_key_len,removed_key,affected_id,remove_count,"
    "delete_ns,rules_per_second,workers,cpu_list,"
    "failed_batch_rejected,failed_batch_unchanged,delete_success,"
    "repeat_delete_rejected,scratch_refresh,stream_stale,stream_reset,"
    "post_delete_block,post_delete_stream,semantic_effect,correctness";

struct Options {
    std::string action;
    std::string root;
    std::string source_dir;
    std::string work_root;
    std::string data_root;
    std::string out_dir;
    std::string machine;
    std::string algorithm;
    std::string cpu_list = "1-95";
    std::string left_result;
    std::string right_result;
    std::string compare_output;
    std::string left_env;
    std::string right_env;
    std::string flamegraph_dir = "/opt/FlameGraph";
    std::string expected_commit;
    std::string hash_counts = "5000000 10000000 50000000 90000000";
    std::string ac_counts = "50000 100000 500000 900000";
    std::string hash_hit_bps = "100 500 1000 5000 9000 10000";
    std::string ac_hit_bps = "100 1000 5000 9000";
    unsigned cpu_core = 1;
    unsigned trials = 1;
    unsigned warmups = 0;
    unsigned query_count = 200000;
    unsigned payload_size = 560;
    unsigned combo_percent = 10;
    unsigned seed = 9202026;
    unsigned quick_rule_count = 500000;
    unsigned quick_hit_bps = 5000;
    unsigned perf_frequency = 199;
    double min_speedup = 1.30;
    bool build_megascan = true;
    bool fresh_build = true;
    bool clean_build = true;
    bool strict_env = true;
    bool require_smt_off = true;
    bool numa_interleave = true;
    bool run_hash = true;
    bool run_ac = true;
    bool run_combo = true;
    bool run_ac_on_combo = true;
    bool run_dynamic_delete = true;
};

static bool parse_unsigned(const char *text, unsigned &value);

static bool exists(const std::string &path)
{
    return access(path.c_str(), F_OK) == 0;
}

static std::string dirname_of(std::string path)
{
    size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? "." : path.substr(0, pos);
}

static int mkdir_p(const std::string &path)
{
    std::string current;
    size_t start = path.size() > 0 && path[0] == '/' ? 1 : 0;

    if (start == 1) {
        current = "/";
    }
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        std::string part = path.substr(start, end - start);
        if (!part.empty()) {
            if (current.size() > 1 && current.back() != '/') {
                current += '/';
            }
            current += part;
            if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
                std::cerr << "创建目录失败：" << current << "："
                          << strerror(errno) << '\n';
                return -1;
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return 0;
}

static int remove_tree(const std::string &path)
{
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return errno == ENOENT ? 0 : -1;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        std::string child = path + "/" + entry->d_name;
        struct stat st;
        if (lstat(child.c_str(), &st) != 0) {
            closedir(dir);
            return -1;
        }
        if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
            if (remove_tree(child) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (unlink(child.c_str()) != 0) {
            closedir(dir);
            return -1;
        }
    }
    closedir(dir);
    return rmdir(path.c_str());
}

static std::vector<char *> command_argv(std::vector<std::string> &command)
{
    std::vector<char *> result;
    for (std::string &value : command) {
        result.push_back(&value[0]);
    }
    result.push_back(nullptr);
    return result;
}

static int run_command(std::vector<std::string> command,
                       const std::string &log_path = std::string(), int cpu = -1)
{
    int pipefd[2] = {-1, -1};
    bool tee = !log_path.empty();
    if (tee && pipe(pipefd) != 0) {
        std::cerr << "创建日志管道失败：" << strerror(errno) << '\n';
        return -1;
    }
    pid_t child = fork();
    if (child == 0) {
        if (cpu >= 0) {
            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(cpu, &set);
            if (sched_setaffinity(0, sizeof(set), &set) != 0) {
                _exit(125);
            }
        }
        if (tee) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
        }
        std::vector<char *> argv = command_argv(command);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    if (child < 0) {
        if (tee) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return -1;
    }
    if (tee) {
        close(pipefd[1]);
        int log = open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        char buffer[8192];
        ssize_t size;
        while ((size = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            (void)write(STDOUT_FILENO, buffer, static_cast<size_t>(size));
            if (log >= 0) {
                (void)write(log, buffer, static_cast<size_t>(size));
            }
        }
        close(pipefd[0]);
        if (log >= 0) {
            close(log);
        }
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static int capture_command(std::vector<std::string> command, std::string &output)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;
    pid_t child = fork();
    if (child == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        std::vector<char *> argv = command_argv(command);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(pipefd[1]);
    char buffer[4096];
    ssize_t size;
    output.clear();
    while ((size = read(pipefd[0], buffer, sizeof(buffer))) > 0)
        output.append(buffer, static_cast<size_t>(size));
    close(pipefd[0]);
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) < 0) return -1;
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static std::string read_text(const std::string &path)
{
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    std::string value = output.str();
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
    return value;
}

static int run_redirect(std::vector<std::string> command, const std::string &path,
                        bool append = false)
{
    pid_t child = fork();
    if (child == 0) {
        int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
        int output = open(path.c_str(), flags, 0644);
        if (output < 0) _exit(126);
        dup2(output, STDOUT_FILENO);
        dup2(output, STDERR_FILENO);
        close(output);
        std::vector<char *> argv = command_argv(command);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static int run_pipeline(std::vector<std::string> left, std::vector<std::string> right,
                        const std::string &output_path)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;
    pid_t first = fork();
    if (first == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        std::vector<char *> argv = command_argv(left);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    pid_t second = fork();
    if (second == 0) {
        int output = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output < 0) _exit(126);
        dup2(pipefd[0], STDIN_FILENO); dup2(output, STDOUT_FILENO);
        close(output); close(pipefd[0]); close(pipefd[1]);
        std::vector<char *> argv = command_argv(right);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(pipefd[0]); close(pipefd[1]);
    int left_status = 0, right_status = 0;
    waitpid(first, &left_status, 0); waitpid(second, &right_status, 0);
    return WIFEXITED(left_status) && WEXITSTATUS(left_status) == 0 &&
           WIFEXITED(right_status) && WEXITSTATUS(right_status) == 0 ? 0 : 1;
}

static int run_generator(std::vector<std::string> args)
{
    pid_t child = fork();
    if (child == 0) {
        std::vector<char *> argv = command_argv(args);
        int result = demo_run_920hg_data_generator(static_cast<int>(args.size()), argv.data());
        fflush(nullptr);
        _exit(result);
    }
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) < 0) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static std::vector<unsigned> number_list(const std::string &text)
{
    std::istringstream input(text);
    std::vector<unsigned> values;
    unsigned value;
    while (input >> value) {
        values.push_back(value);
    }
    return values;
}

static std::string rate_label(unsigned bps)
{
    char text[32];
    snprintf(text, sizeof(text), "%.2f", static_cast<double>(bps) / 10000.0);
    return text;
}

static int generate_one(const std::string &algorithm, unsigned rules,
                        unsigned combos, unsigned queries, unsigned bps,
                        const Options &options, const std::string &rules_path,
                        const std::string &corpus_path)
{
    std::vector<std::string> args = {"gen_920hg_data", "-a", algorithm,
        "-n", std::to_string(rules), "-S", std::to_string(options.seed)};
    if (combos > 0) {
        args.insert(args.end(), {"-c", std::to_string(combos)});
    }
    if (queries > 0) {
        args.insert(args.end(), {"-q", std::to_string(queries), "-H",
            std::to_string(bps)});
    }
    if (algorithm != "hash") {
        args.insert(args.end(), {"-L", std::to_string(options.payload_size)});
    }
    if (!rules_path.empty()) {
        args.insert(args.end(), {"-t", rules_path});
    }
    if (!corpus_path.empty()) {
        args.insert(args.end(), {"-s", corpus_path});
    }
    return run_generator(args);
}

static void write_metadata(const std::string &path, const std::string &algorithm,
                           unsigned rules, unsigned atomic, unsigned combos,
                           unsigned bps, const Options &options, unsigned domain)
{
    std::ofstream out(path);
    out << "FORMAT_VERSION=4\nALGORITHM=" << algorithm
        << "\nRULE_COUNT=" << rules << "\nATOMIC_RULE_COUNT=" << atomic
        << "\nCOMBINATION_RULE_COUNT=" << combos
        << "\nQUERY_RULE_DOMAIN=" << domain
        << "\nQUERY_COUNT=" << options.query_count << "\nHIT_BPS=" << bps
        << "\nHIT_RATE=" << rate_label(bps)
        << "\nEXPECTED_HITS=" << options.query_count * bps / 10000
        << "\nPAYLOAD_SIZE=" << (algorithm == "hash" ? 16 : options.payload_size)
        << "\nSEED=" << options.seed << '\n';
}

static int generate_matrix(const Options &options)
{
    std::vector<unsigned> hash_counts = number_list(options.hash_counts);
    std::vector<unsigned> ac_counts = number_list(options.ac_counts);
    std::vector<unsigned> hash_bps = number_list(options.hash_hit_bps);
    std::vector<unsigned> ac_bps = number_list(options.ac_hit_bps);
    if (hash_counts.empty() || ac_counts.empty() || hash_bps.empty() || ac_bps.empty()) {
        std::cerr << "规则数量或命中率列表为空。\n";
        return 1;
    }
    for (const char *dir : {"shared", "hash", "ac", "accombo"}) {
        if (mkdir_p(options.data_root + "/" + dir) != 0) {
            return 1;
        }
    }
    unsigned hash_domain = *std::min_element(hash_counts.begin(), hash_counts.end());
    unsigned hash_max = *std::max_element(hash_counts.begin(), hash_counts.end());
    unsigned ac_domain = *std::min_element(ac_counts.begin(), ac_counts.end());
    unsigned base_combos = ac_domain * options.combo_percent / 100;

    std::ostringstream config_text;
    config_text << "FORMAT_VERSION=4\nHASH_RULE_COUNTS=" << options.hash_counts
                << "\nAC_RULE_COUNTS=" << options.ac_counts
                << "\nHASH_HIT_BPS_LIST=" << options.hash_hit_bps
                << "\nAC_HIT_BPS_LIST=" << options.ac_hit_bps
                << "\nQUERY_COUNT=" << options.query_count
                << "\nPAYLOAD_SIZE=" << options.payload_size
                << "\nCOMBO_PERCENT=" << options.combo_percent
                << "\nSEED=" << options.seed;
    std::string config_path = options.data_root + "/suite_config.env";
    if (exists(config_path) && read_text(config_path) != config_text.str()) {
        std::cerr << "数据目录中已有不同配置，请更换 --data-root。\n";
        return 1;
    }
    std::ofstream config(config_path);
    config << config_text.str() << '\n';

    std::ofstream manifest(options.data_root + "/manifest.csv");
    manifest << "algorithm,rule_count,atomic_rule_count,combination_rule_count,"
                "query_count,hit_bps,hit_rate,payload_size,seed,rules_file,corpus_file\n";

    for (unsigned bps : hash_bps) {
        std::string corpus = options.data_root + "/shared/hash_q" +
            std::to_string(options.query_count) + "_h" + std::to_string(bps) + ".bin";
        if (!exists(corpus) && generate_one("hash", hash_domain, 0,
                options.query_count, bps, options, "", corpus) != 0) return 1;
    }
    for (unsigned bps : ac_bps) {
        std::string base = "_q" + std::to_string(options.query_count) + "_p" +
            std::to_string(options.payload_size) + "_h" + std::to_string(bps) + ".bin";
        std::string ac_corpus = options.data_root + "/shared/ac" + base;
        std::string combo_corpus = options.data_root + "/shared/accombo" + base;
        if (!exists(ac_corpus) && generate_one("ac", ac_domain, 0,
                options.query_count, bps, options, "", ac_corpus) != 0) return 1;
        if (!exists(combo_corpus) && generate_one("accombo", ac_domain, base_combos,
                options.query_count, bps, options, "", combo_corpus) != 0) return 1;
    }

    std::string hash_rules = options.data_root + "/shared/hash_" +
        std::to_string(hash_max) + "_rules.bin";
    if (!exists(hash_rules) && generate_one("hash", hash_max, 0, 0, 0,
            options, hash_rules, "") != 0) return 1;
    struct stat st;
    if (stat(hash_rules.c_str(), &st) != 0 ||
        static_cast<unsigned long long>(st.st_size) !=
            static_cast<unsigned long long>(hash_max) * 16) {
        std::cerr << "HASH 规则文件大小错误：" << hash_rules << '\n';
        return 1;
    }

    for (unsigned rules : hash_counts) {
        for (unsigned bps : hash_bps) {
            std::string rate = rate_label(bps);
            std::string dataset = options.data_root + "/hash/hash_" +
                std::to_string(rules) + "_" + std::to_string(options.query_count) + "_" + rate;
            mkdir_p(dataset);
            write_metadata(dataset + "/metadata.env", "hash", rules, rules, 0,
                           bps, options, hash_domain);
            manifest << "hash," << rules << ',' << rules << ",0,"
                     << options.query_count << ',' << bps << ',' << rate << ",16,"
                     << options.seed << ",shared/hash_" << hash_max
                     << "_rules.bin,shared/hash_q" << options.query_count
                     << "_h" << bps << ".bin\n";
        }
    }

    for (unsigned rules : ac_counts) {
        std::string ac_name = "ac_" + std::to_string(rules) + "_rules.tsv";
        std::string ac_rules = options.data_root + "/shared/" + ac_name;
        if (!exists(ac_rules) && generate_one("ac", rules, 0, 0, 0,
                options, ac_rules, "") != 0) return 1;
        for (unsigned bps : ac_bps) {
            std::string rate = rate_label(bps);
            std::string dataset = options.data_root + "/ac/ac_" +
                std::to_string(rules) + "_" + std::to_string(options.query_count) + "_" + rate;
            mkdir_p(dataset);
            write_metadata(dataset + "/metadata.env", "ac", rules, rules, 0,
                           bps, options, ac_domain);
            manifest << "ac," << rules << ',' << rules << ",0,"
                     << options.query_count << ',' << bps << ',' << rate << ','
                     << options.payload_size << ',' << options.seed << ",shared/"
                     << ac_name << ",shared/ac_q" << options.query_count << "_p"
                     << options.payload_size << "_h" << bps << ".bin\n";
        }
        unsigned combos = rules * options.combo_percent / 100;
        unsigned atomic = rules - combos;
        std::string combo_name = "accombo_" + std::to_string(rules) + "_rules.tsv";
        std::string combo_rules = options.data_root + "/shared/" + combo_name;
        if (!exists(combo_rules) && generate_one("accombo", rules, combos, 0, 0,
                options, combo_rules, "") != 0) return 1;
        for (unsigned bps : ac_bps) {
            std::string rate = rate_label(bps);
            std::string dataset = options.data_root + "/accombo/accombo_" +
                std::to_string(rules) + "_" + std::to_string(options.query_count) + "_" + rate;
            mkdir_p(dataset);
            write_metadata(dataset + "/metadata.env", "accombo", rules, atomic,
                           combos, bps, options, ac_domain);
            manifest << "accombo," << rules << ',' << atomic << ',' << combos << ','
                     << options.query_count << ',' << bps << ',' << rate << ','
                     << options.payload_size << ',' << options.seed << ",shared/"
                     << combo_name << ",shared/accombo_q" << options.query_count
                     << "_p" << options.payload_size << "_h" << bps << ".bin\n";
        }
    }
    std::cout << "数据生成完成：" << options.data_root << '\n';
    return 0;
}

static std::vector<std::string> split(const std::string &text, char delimiter)
{
    std::vector<std::string> values;
    std::istringstream input(text);
    std::string value;
    while (std::getline(input, value, delimiter)) values.push_back(value);
    return values;
}

static int compile_benchmark(const Options &options, const std::string &binary)
{
    std::string build = options.root + "/build";
    std::string marker = options.out_dir + "/.build_started";
    if (options.build_megascan) {
        { std::ofstream started(marker); }
        if (options.fresh_build && remove_tree(build) != 0 && errno != ENOENT) {
            std::cerr << "清理构建目录失败：" << build << '\n';
            return 1;
        }
        if (run_command({"cmake", "-S", options.root, "-B", build,
                         "-DCMAKE_BUILD_TYPE=Release", "-DMS_ENABLE_PROFILING=OFF"}) != 0) return 1;
        std::vector<std::string> command = {"cmake", "--build", build, "--parallel"};
        const char *jobs = getenv("BUILD_JOBS");
        command.push_back(jobs == nullptr ? std::to_string(sysconf(_SC_NPROCESSORS_ONLN)) : jobs);
        if (options.clean_build) command.push_back("--clean-first");
        if (run_command(command) != 0) return 1;
    }
    std::string lib = options.root + "/output/lib/libms.so";
    if (!exists(lib)) {
        std::cerr << "找不到 libms.so：" << lib << '\n';
        return 1;
    }
    int result = run_command({"gcc", "-O2", "-DNDEBUG", "-std=gnu11", "-Wall",
        "-Wextra", "-Werror", options.source_dir + "/bench_920hg.c",
        "-I" + options.root + "/ms/include", "-I" + options.root + "/output/include",
        "-L" + options.root + "/output/lib",
        "-Wl,-rpath," + options.root + "/output/lib", "-lms", "-pthread", "-o", binary});
    if (result != 0) return result;
    std::string symbols;
    if (capture_command({"nm", "-a", lib}, symbols) != 0 ||
        symbols.find("ms_scratch_load_stream_state") != std::string::npos ||
        symbols.find("ms_scratch_commit_stream_state") != std::string::npos) {
        std::cerr << "检测到旧 stream-copy 符号或无法检查 libms.so。\n";
        return 1;
    }
    std::string linked;
    if (capture_command({"ldd", binary}, linked) != 0) return 1;
    size_t name = linked.find("libms.so => ");
    if (name == std::string::npos) {
        std::cerr << "ldd 未解析到 libms.so。\n";
        return 1;
    }
    size_t begin = name + strlen("libms.so => ");
    size_t end = linked.find(' ', begin);
    std::string resolved = linked.substr(begin, end - begin);
    char actual_path[4096], expected_path[4096];
    if (realpath(resolved.c_str(), actual_path) == nullptr ||
        realpath(lib.c_str(), expected_path) == nullptr || strcmp(actual_path, expected_path) != 0) {
        std::cerr << "benchmark 链接到了非预期 libms.so：" << resolved << '\n';
        return 1;
    }
    return 0;
}

static int validate_source_tree(const Options &options)
{
    std::string commit;
    if (capture_command({"git", "-c", "safe.directory=" + options.root, "-C",
                         options.root, "rev-parse", "HEAD"}, commit) != 0) {
        std::cerr << "Megascan 不是有效 Git 工作区。\n";
        return 1;
    }
    if (!options.expected_commit.empty() && commit != options.expected_commit) {
        std::cerr << "Megascan 提交不匹配：实际=" << commit
                  << " 期望=" << options.expected_commit << '\n';
        return 1;
    }
    return 0;
}

static int validate_environment(const Options &options, bool full)
{
    if (validate_source_tree(options) != 0) return 1;
    if (!full && options.cpu_core == 0) {
        std::cerr << "单核测试不能使用 CPU 0。\n";
        return 1;
    }
    if (!full) {
        std::string cpu_path = "/sys/devices/system/cpu/cpu" + std::to_string(options.cpu_core);
        if (!exists(cpu_path) || (exists(cpu_path + "/online") && read_text(cpu_path + "/online") != "1")) {
            std::cerr << "指定的 CPU 不存在或未在线：" << options.cpu_core << '\n';
            return 1;
        }
    } else {
        std::set<unsigned> cpus;
        std::istringstream input(options.cpu_list);
        std::string item;
        while (std::getline(input, item, ',')) {
            size_t dash = item.find('-');
            unsigned first = 0, last = 0;
            if (dash == std::string::npos) {
                if (!parse_unsigned(item.c_str(), first)) return 1;
                last = first;
            } else if (!parse_unsigned(item.substr(0, dash).c_str(), first) ||
                       !parse_unsigned(item.substr(dash + 1).c_str(), last) || last < first) {
                std::cerr << "CPU 列表格式错误：" << options.cpu_list << '\n';
                return 1;
            }
            for (unsigned cpu = first; cpu <= last; cpu++) {
                std::string cpu_path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu);
                if (cpu == 0 || !cpus.insert(cpu).second || !exists(cpu_path) ||
                    (exists(cpu_path + "/online") && read_text(cpu_path + "/online") != "1")) {
                    std::cerr << "CPU 列表包含 CPU 0、重复项或离线 CPU：" << cpu << '\n';
                    return 1;
                }
                if (cpu == 0xffffffffU) break;
            }
        }
        if (cpus.empty()) return 1;
    }
    if (options.strict_env && read_text("/proc/sys/kernel/numa_balancing") != "0") {
        std::cerr << "NUMA automatic balancing 未关闭。\n";
        return 1;
    }
    if (full && options.require_smt_off &&
        read_text("/sys/devices/system/cpu/smt/active") == "1") {
        std::cerr << "SMT 未关闭，不能执行整机等物理核测试。\n";
        return 1;
    }
    if (options.strict_env) {
        DIR *cpus = opendir("/sys/devices/system/cpu");
        bool bad_governor = false;
        if (cpus != nullptr) {
            struct dirent *entry;
            while ((entry = readdir(cpus)) != nullptr) {
                if (strncmp(entry->d_name, "cpu", 3) != 0 || entry->d_name[3] < '0' || entry->d_name[3] > '9') continue;
                std::string governor = read_text(std::string("/sys/devices/system/cpu/") +
                    entry->d_name + "/cpufreq/scaling_governor");
                if (!governor.empty() && governor != "performance") bad_governor = true;
            }
            closedir(cpus);
        }
        if (bad_governor) {
            std::cerr << "CPU governor 不是 performance。\n";
            return 1;
        }
    }
    return 0;
}

static int write_environment_log(const Options &options, bool full)
{
    std::string commit, hash;
    if (capture_command({"git", "-c", "safe.directory=" + options.root, "-C",
                         options.root, "rev-parse", "HEAD"}, commit) != 0) return 1;
    std::ofstream env(options.out_dir + "/env.log");
    if (!env) return 1;
    env << "MACHINE_TAG=" << options.machine << "\nMEGASCAN_ROOT=" << options.root
        << "\nDATA_ROOT=" << options.data_root << '\n'
        << (full ? "CPU_LIST=" + options.cpu_list : "CPU_CORE=" + std::to_string(options.cpu_core))
        << "\nTRIALS=" << options.trials << "\nWARMUPS=" << options.warmups
        << "\nMEGASCAN_COMMIT=" << commit << "\nSOURCE_DIRTY=0\n";
    const std::vector<std::pair<std::string, std::string>> files = {
        {"ms_scratch_source_sha256", options.root + "/ms/src/ms_scratch.c"},
        {"ms_stream_source_sha256", options.root + "/ms/src/ms_stream.c"},
        {"ms_acdb_source_sha256", options.root + "/ms/src/ms_acdb.c"},
        {"ms_database_source_sha256", options.root + "/ms/src/ms_database.c"},
        {"bench_source_sha256", options.source_dir + "/bench_920hg.c"},
        {"driver_source_sha256", options.root + "/test/demo/workflow_runner.cpp"},
        {"summarize_source_sha256", options.root + "/test/demo/workflow_results.cpp"},
        {"manifest_sha256", options.data_root + "/manifest.csv"}};
    for (const auto &file : files) {
        if (capture_command({"sha256sum", file.second}, hash) != 0) return 1;
        size_t space = hash.find(' ');
        env << file.first << '=' << hash.substr(0, space) << '\n';
    }
    env << "timing_excludes=corpus_load,database_compile,worker_create,scratch_alloc,stream_open,correctness,warmup\n"
        << "stream_full=reset_plus_one_scan_per_query\n"
        << "dynamic_delete=one_fresh_database_per_trial,predelete_validation,"
           "atomic_failure_check,timed_delete,stale_reset,postdelete_validation\n";
    if (full) env << "throughput_scope=aggregate_bytes_over_wall_clock\n";
    return 0;
}

static int benchmark_matrix(const Options &options, bool full, bool quick = false)
{
    std::string manifest_path = options.data_root + "/manifest.csv";
    std::ifstream manifest(manifest_path);
    if (!manifest) {
        std::cerr << "找不到 manifest：" << manifest_path << '\n';
        return 1;
    }
    if ((quick ? validate_source_tree(options) : validate_environment(options, full)) != 0) return 1;
    if (mkdir_p(options.out_dir + "/bin") != 0) return 1;
    std::string raw = options.out_dir + "/raw_trials.csv";
    std::string delete_raw = options.out_dir + "/raw_delete_trials.csv";
    if (exists(raw) || exists(delete_raw)) {
        std::cerr << "结果已存在，拒绝覆盖："
                  << (exists(raw) ? raw : delete_raw) << '\n';
        return 1;
    }
    std::string binary = options.out_dir + "/bin/bench_920hg";
    if (compile_benchmark(options, binary) != 0) return 1;
    if (write_environment_log(options, full) != 0) return 1;
    {
        std::ifstream source(manifest_path, std::ios::binary);
        std::ofstream target(options.out_dir + "/manifest.csv", std::ios::binary);
        target << source.rdbuf();
    }
    std::ofstream csv(raw);
    csv << RAW_HEADER << '\n';
    csv.close();
    std::ofstream delete_csv(delete_raw);
    delete_csv << RAW_DELETE_HEADER << '\n';
    delete_csv.close();

    std::string old_ld = getenv("LD_LIBRARY_PATH") == nullptr ? "" : getenv("LD_LIBRARY_PATH");
    std::string ld = options.root + "/output/lib" + (old_ld.empty() ? "" : ":" + old_ld);
    setenv("LD_LIBRARY_PATH", ld.c_str(), 1);

    std::string header;
    std::getline(manifest, header);
    std::string line;
    bool ran_case = false;
    while (std::getline(manifest, line)) {
        std::vector<std::string> field = split(line, ',');
        if (field.size() != 11) {
            std::cerr << "manifest 行格式错误：" << line << '\n';
            return 1;
        }
        std::vector<std::string> algorithms;
        if (quick && (field[1] != std::to_string(options.quick_rule_count) ||
                      field[5] != std::to_string(options.quick_hit_bps))) continue;
        auto selected = [&](const char *name) {
            return options.algorithm.empty() || options.algorithm == name;
        };
        if (!quick && field[0] == "hash" && options.run_hash) algorithms.push_back("hash");
        if (quick && field[0] == "hash" && options.run_hash && selected("hash")) {
            algorithms.push_back("hash");
        }
        if (field[0] == "ac") {
            if (quick) {
                if (options.run_ac && selected("ac")) algorithms.push_back("ac");
            } else {
                if (options.run_ac) algorithms.push_back("ac");
            }
            if (options.run_ac && options.run_dynamic_delete && selected("ac_delete")) {
                algorithms.push_back("ac_delete");
            }
        }
        if (field[0] == "accombo") {
            if (options.run_combo && selected("accombo")) algorithms.push_back("accombo");
            if (options.run_ac_on_combo && selected("ac_on_combo")) algorithms.push_back("ac_on_combo");
            if (options.run_combo && options.run_dynamic_delete) {
                if (selected("accombo_delete")) algorithms.push_back("accombo_delete");
                if (selected("accombo_combination_delete")) {
                    algorithms.push_back("accombo_combination_delete");
                }
            }
        }
        for (const std::string &algorithm : algorithms) {
            bool dynamic_delete =
                algorithm == "ac_delete" ||
                algorithm == "accombo_delete" ||
                algorithm == "accombo_combination_delete";
            unsigned process_trials = dynamic_delete ? options.trials : 1u;
            for (unsigned trial = 1; trial <= process_trials; trial++) {
                std::vector<std::string> command;
                if (full && options.numa_interleave) {
                    command.insert(command.end(), {"numactl", "--interleave=all"});
                }
                command.insert(command.end(), {binary, "-a", algorithm, "-M",
                    algorithm == "hash" ? "lookup" : "both", "-n", field[1],
                    "-t", options.data_root + "/" + field[9],
                    "-s", options.data_root + "/" + field[10], "-p", raw,
                    "-T", options.machine, "-R",
                    dynamic_delete ? "1" : std::to_string(options.trials),
                    "-w", std::to_string(options.warmups)});
                if (dynamic_delete) {
                    command.insert(command.end(), {"-d", delete_raw, "-I",
                        std::to_string(trial)});
                }
                if (full) command.insert(command.end(), {"-c", options.cpu_list});
                std::cout << "运行：算法=" << algorithm << " 规则=" << field[1]
                          << " 命中率=" << field[6];
                if (dynamic_delete) {
                    std::cout << " 删除延迟轮次=" << trial;
                }
                std::cout << '\n';
                if (run_command(command, options.out_dir + "/run.log",
                                full ? -1 : static_cast<int>(options.cpu_core)) != 0) {
                    return 1;
                }
                ran_case = true;
            }
        }
    }
    if (!options.algorithm.empty() && !ran_case) {
        std::cerr << "找不到指定测试项：算法=" << options.algorithm
                  << " 规则=" << options.quick_rule_count
                  << " 命中率(bps)=" << options.quick_hit_bps << '\n';
        return 1;
    }
    std::cout << "原始测试完成：" << raw << '\n';
    int summary = demo_920hg_summarize(options.out_dir);
    if (summary == 2) {
        std::cerr << "警告：正确性或性能标记存在 FAIL，结果文件已完整输出。\n";
    }
    return summary == 1 ? 1 : 0;
}

static bool find_manifest_case(const Options &options, const std::string &algorithm,
                               std::vector<std::string> &field)
{
    std::ifstream manifest(options.data_root + "/manifest.csv");
    std::string line;
    std::getline(manifest, line);
    while (std::getline(manifest, line)) {
        std::vector<std::string> candidate = split(line, ',');
        if (candidate.size() == 11 && candidate[0] == algorithm &&
            candidate[1] == std::to_string(options.quick_rule_count) &&
            candidate[5] == std::to_string(options.quick_hit_bps)) {
            field = candidate;
            return true;
        }
    }
    return false;
}

static int compile_profile_benchmark(const Options &options, const std::string &binary)
{
    std::string build = options.root + "/build-profile";
    if (options.fresh_build && remove_tree(build) != 0 && errno != ENOENT) return 1;
    if (run_command({"cmake", "-S", options.root, "-B", build,
                     "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DMS_ENABLE_PROFILING=ON"}) != 0) return 1;
    const char *jobs = getenv("BUILD_JOBS");
    if (run_command({"cmake", "--build", build, "--parallel",
                     jobs == nullptr ? std::to_string(sysconf(_SC_NPROCESSORS_ONLN)) : jobs}) != 0) return 1;
    return run_command({"gcc", "-O2", "-g3", "-fno-omit-frame-pointer", "-DNDEBUG",
        "-std=gnu11", "-Wall", "-Wextra", "-Werror",
        options.source_dir + "/bench_920hg.c", "-I" + options.root + "/ms/include",
        "-I" + options.root + "/output/include", "-L" + options.root + "/output/lib",
        "-Wl,-rpath," + options.root + "/output/lib", "-lms", "-pthread", "-o", binary});
}

static int profile_matrix(const Options &options)
{
    if (validate_source_tree(options) != 0) return 1;
    if (!exists(options.flamegraph_dir + "/stackcollapse-perf.pl") ||
        !exists(options.flamegraph_dir + "/flamegraph.pl")) {
        std::cerr << "找不到 FlameGraph 工具，请使用 --flamegraph-dir 指定。\n";
        return 1;
    }
    if (mkdir_p(options.out_dir + "/bin") != 0) return 1;
    std::string binary = options.out_dir + "/bin/bench_920hg";
    if (compile_profile_benchmark(options, binary) != 0) return 1;
    if (write_environment_log(options, false) != 0) return 1;
    std::string old_ld = getenv("LD_LIBRARY_PATH") == nullptr ? "" : getenv("LD_LIBRARY_PATH");
    std::string ld = options.root + "/output/lib" + (old_ld.empty() ? "" : ":" + old_ld);
    setenv("LD_LIBRARY_PATH", ld.c_str(), 1);

    for (const std::string &algorithm : {std::string("ac"), std::string("accombo")}) {
        std::vector<std::string> field;
        if (!find_manifest_case(options, algorithm, field)) {
            std::cerr << "找不到 profile 数据：算法=" << algorithm << '\n';
            return 1;
        }
        for (const std::string &mode : {std::string("block"), std::string("stream-full")}) {
            std::string case_dir = options.out_dir + "/" + algorithm + "_" + mode;
            mkdir_p(case_dir);
            std::ofstream raw(case_dir + "/raw_trials.csv"); raw << RAW_HEADER << '\n'; raw.close();
            pid_t bench = fork();
            if (bench == 0) {
                cpu_set_t set; CPU_ZERO(&set); CPU_SET(options.cpu_core, &set);
                if (sched_setaffinity(0, sizeof(set), &set) != 0) _exit(125);
                setenv("MS_BENCH_PROFILE_STOP", "1", 1);
                int log = open((case_dir + "/bench.log").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (log < 0) _exit(126);
                dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log);
                std::vector<std::string> command = {binary, "-a", algorithm, "-M", mode,
                    "-n", field[1], "-t", options.data_root + "/" + field[9],
                    "-s", options.data_root + "/" + field[10], "-p",
                    case_dir + "/raw_trials.csv", "-T", options.machine,
                    "-R", std::to_string(options.trials), "-w", std::to_string(options.warmups)};
                std::vector<char *> argv = command_argv(command);
                execv(argv[0], argv.data()); _exit(127);
            }
            int stopped = 0;
            bool attached = false;
            for (unsigned attempt = 0; bench > 0 && attempt < 600; attempt++) {
                pid_t state = waitpid(bench, &stopped, WUNTRACED | WNOHANG);
                if (state == bench && WIFSTOPPED(stopped)) { attached = true; break; }
                if (state == bench && WIFEXITED(stopped)) break;
                usleep(100000);
            }
            if (!attached) {
                std::cerr << "benchmark 未在 profiler attach 点停止。\n";
                return 1;
            }
            std::string perf_data = case_dir + "/perf.data";
            pid_t perf = fork();
            if (perf == 0) {
                int log = open((case_dir + "/perf.log").c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (log >= 0) { dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log); }
                std::vector<std::string> command = {"perf", "record", "-F",
                    std::to_string(options.perf_frequency), "-e", "cycles:u", "-g",
                    "--call-graph", "dwarf,16384", "-o", perf_data, "-p", std::to_string(bench)};
                std::vector<char *> argv = command_argv(command);
                execvp(argv[0], argv.data()); _exit(127);
            }
            sleep(1);
            kill(bench, SIGCONT);
            int bench_status = 0, perf_status = 0;
            waitpid(bench, &bench_status, 0); waitpid(perf, &perf_status, 0);
            if (!WIFEXITED(bench_status) || WEXITSTATUS(bench_status) != 0) return 1;
            if (run_redirect({"perf", "report", "--stdio", "--no-children", "-i", perf_data},
                             case_dir + "/perf_report.txt") != 0) return 1;
            if (run_pipeline({"perf", "script", "-i", perf_data},
                             {"perl", options.flamegraph_dir + "/stackcollapse-perf.pl"},
                             case_dir + "/stacks.folded") != 0) return 1;
            std::string title = options.machine + " " + algorithm + " " + mode +
                " rules=" + field[1] + " hit=" + field[6];
            if (run_redirect({"perl", options.flamegraph_dir + "/flamegraph.pl", "--title",
                              title, "--countname", "cycles", case_dir + "/stacks.folded"},
                             case_dir + "/flamegraph.svg") != 0) return 1;
        }
    }
    std::cout << "profile 完成：" << options.out_dir << '\n';
    return 0;
}

static bool parse_unsigned(const char *text, unsigned &value)
{
    char *end = nullptr;
    errno = 0;
    unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > 0xffffffffUL) return false;
    value = static_cast<unsigned>(parsed);
    return true;
}

static void usage()
{
    std::cout <<
        "用法：megascan_demo 920hg <操作> [选项]\n\n"
        "操作：\n"
        "  generate          内部生成完整规则和 corpus 矩阵\n"
        "  single            内部执行原口径单核全量测试\n"
        "  full              内部执行原口径整机全量测试\n"
        "  quick             内部执行一组 AC 快速检查\n"
        "  profile           内部执行 AC perf profile\n"
        "  summarize         内部整理原始 CSV\n"
        "  compare-single    内部比较单核 920/HG 结果\n"
        "  compare-full      内部比较整机 920/HG 结果\n\n"
        "常用选项：\n"
        "  --machine 920|hg       机器标识\n"
        "  --data-root <目录>     数据目录\n"
        "  --work-root <目录>     工作目录\n"
        "  --out-dir <目录>       结果目录\n"
        "  --cpu-core <编号>      单核 CPU，默认 1\n"
        "  --cpu-list <列表>      整机 CPU，默认 1-95\n"
        "  --trials <次数>        默认 1\n"
        "  --warmups <次数>       默认 0\n"
        "  --algorithm <算法>     quick 只运行指定算法\n"
        "  --no-build             不重新构建 Megascan\n"
        "  --keep-build           不删除现有 build 目录\n"
        "  --skip-hash|--skip-ac|--skip-accombo|--skip-ac-on-combo\n"
        "  --skip-dynamic-delete  跳过真实规则库动态删除测试项\n"
        "  -h, --help             显示帮助\n";
}

static void action_usage(const std::string &action)
{
    if (action == "generate") {
        std::cout << "用法：megascan_demo 920hg generate [数据选项]\n"
                     "  --data-root <目录> --hash-rule-counts <列表> --ac-rule-counts <列表>\n"
                     "  --hash-hit-bps <列表> --ac-hit-bps <列表> --query-count <数量>\n"
                     "  --payload-size <字节> --combo-percent <百分数> --seed <整数>\n";
    } else if (action == "single") {
        std::cout << "用法：megascan_demo 920hg single --machine 920|hg [测试选项]\n"
                     "  --data-root <目录> --out-dir <目录> --cpu-core <编号>\n"
                     "  --trials <奇数> --warmups <次数> --no-build --keep-build --no-clean\n"
                     "  --skip-hash --skip-ac --skip-accombo --skip-ac-on-combo\n"
                     "  --skip-dynamic-delete\n";
    } else if (action == "full") {
        std::cout << "用法：megascan_demo 920hg full --machine 920|hg [测试选项]\n"
                     "  --data-root <目录> --out-dir <目录> --cpu-list <列表>\n"
                     "  --trials <奇数> --warmups <次数> --no-numa-interleave --allow-smt\n"
                     "  --skip-dynamic-delete\n";
    } else if (action == "quick") {
        std::cout << "用法：megascan_demo 920hg quick --machine 920|hg [选项]\n"
                     "  --algorithm <算法>  只运行 hash、ac、accombo、\n"
                     "                      ac_on_combo 或指定动态删除算法\n"
                     "  --rule-count <数量> --hit-rate <0..1> 或 --hit-bps <0..10000>\n"
                     "  --cpu-core <编号> --data-root <目录> --out-dir <目录>\n"
                     "  --skip-dynamic-delete\n";
    } else if (action == "profile") {
        std::cout << "用法：megascan_demo 920hg profile --machine 920|hg [选项]\n"
                     "  --rule-count <数量> --hit-rate <0..1> --cpu-core <编号>\n"
                     "  --perf-frequency <频率> --flamegraph-dir <目录>\n";
    } else if (action == "summarize") {
        std::cout << "用法：megascan_demo 920hg summarize --out-dir <结果目录>\n"
                     "读取 raw_trials.csv 和 raw_delete_trials.csv，生成扫描与动态删除汇总。\n";
    } else if (action == "compare-single" || action == "compare-full") {
        std::cout << "用法：megascan_demo 920hg " << action << " [比较选项]\n"
                     "  --920-result <summary.csv> --hg-result <summary.csv>\n"
                     "  --920-env <env.log> --hg-env <env.log>\n"
                     "  --min-speedup <比值> --output <CSV>\n";
    } else {
        usage();
    }
}

static bool locate_source(Options &options)
{
    const char *root = getenv("MEGASCAN_ROOT");
    if (root != nullptr) options.root = root;
    char executable[4096];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (options.root.empty() && length > 0) {
        executable[length] = '\0';
        std::string bin_dir = dirname_of(executable);
        char resolved[4096];
        std::string candidate = bin_dir + "/../../..";
        if (realpath(candidate.c_str(), resolved) != nullptr) options.root = resolved;
    }
    if (options.root.empty()) return false;
    options.source_dir = options.root + "/test/demo/920hg";
    return exists(options.source_dir + "/bench_920hg.c");
}

static int parse_options(int argc, char **argv, Options &options)
{
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage();
        return argc < 2 ? 1 : 2;
    }
    options.action = argv[1];
    const char *home = getenv("HOME");
    const char *xdg_cache = getenv("XDG_CACHE_HOME");
    const char *work_root = getenv("WORK_ROOT");
    const char *data_root = getenv("DATA_ROOT");
    options.work_root = work_root != nullptr ? work_root :
        (xdg_cache != nullptr ? std::string(xdg_cache) :
         std::string(home == nullptr ? "." : home) + "/.cache") + "/megascan/920hg";
    bool data_explicit = data_root != nullptr;
    if (data_explicit) options.data_root = data_root;
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        auto value = [&](std::string &target) -> bool {
            if (++i >= argc) return false;
            target = argv[i];
            return true;
        };
        if (arg == "-h" || arg == "--help") { action_usage(options.action); return 2; }
        else if (arg == "--machine") { if (!value(options.machine)) return 1; }
        else if (arg == "--algorithm") {
            if (!value(options.algorithm)) return 1;
        }
        else if (arg == "--work-root") { if (!value(options.work_root)) return 1; }
        else if (arg == "--data-root") { if (!value(options.data_root)) return 1; data_explicit = true; }
        else if (arg == "--out-dir") { if (!value(options.out_dir)) return 1; }
        else if (arg == "--cpu-list") { if (!value(options.cpu_list)) return 1; }
        else if (arg == "--920-result") { if (!value(options.left_result)) return 1; }
        else if (arg == "--hg-result") { if (!value(options.right_result)) return 1; }
        else if (arg == "--output") { if (!value(options.compare_output)) return 1; }
        else if (arg == "--920-env") { if (!value(options.left_env)) return 1; }
        else if (arg == "--hg-env") { if (!value(options.right_env)) return 1; }
        else if (arg == "--flamegraph-dir") { if (!value(options.flamegraph_dir)) return 1; }
        else if (arg == "--expected-commit") { if (!value(options.expected_commit)) return 1; }
        else if (arg == "--hash-rule-counts") { if (!value(options.hash_counts)) return 1; }
        else if (arg == "--ac-rule-counts") { if (!value(options.ac_counts)) return 1; }
        else if (arg == "--hash-hit-bps") { if (!value(options.hash_hit_bps)) return 1; }
        else if (arg == "--ac-hit-bps") { if (!value(options.ac_hit_bps)) return 1; }
        else if (arg == "--cpu-core") { if (++i >= argc || !parse_unsigned(argv[i], options.cpu_core)) return 1; }
        else if (arg == "--trials") { if (++i >= argc || !parse_unsigned(argv[i], options.trials)) return 1; }
        else if (arg == "--warmups") { if (++i >= argc || !parse_unsigned(argv[i], options.warmups)) return 1; }
        else if (arg == "--query-count") { if (++i >= argc || !parse_unsigned(argv[i], options.query_count)) return 1; }
        else if (arg == "--payload-size") { if (++i >= argc || !parse_unsigned(argv[i], options.payload_size)) return 1; }
        else if (arg == "--combo-percent") { if (++i >= argc || !parse_unsigned(argv[i], options.combo_percent)) return 1; }
        else if (arg == "--seed") { if (++i >= argc || !parse_unsigned(argv[i], options.seed)) return 1; }
        else if (arg == "--rule-count") { if (++i >= argc || !parse_unsigned(argv[i], options.quick_rule_count)) return 1; }
        else if (arg == "--hit-bps") { if (++i >= argc || !parse_unsigned(argv[i], options.quick_hit_bps)) return 1; }
        else if (arg == "--hit-rate") {
            if (++i >= argc) return 1;
            char *end = nullptr;
            double rate = std::strtod(argv[i], &end);
            if (end == argv[i] || *end != '\0' || rate < 0.0 || rate > 1.0) return 1;
            options.quick_hit_bps = static_cast<unsigned>(rate * 10000.0 + 0.5);
        }
        else if (arg == "--perf-frequency") { if (++i >= argc || !parse_unsigned(argv[i], options.perf_frequency)) return 1; }
        else if (arg == "--min-speedup") {
            if (++i >= argc) return 1;
            char *end = nullptr;
            options.min_speedup = std::strtod(argv[i], &end);
            if (end == argv[i] || *end != '\0' || options.min_speedup <= 0.0) return 1;
        }
        else if (arg == "--no-build") options.build_megascan = false;
        else if (arg == "--keep-build") options.fresh_build = false;
        else if (arg == "--no-clean") options.clean_build = false;
        else if (arg == "--no-strict-env") options.strict_env = false;
        else if (arg == "--allow-smt") options.require_smt_off = false;
        else if (arg == "--no-numa-interleave") options.numa_interleave = false;
        else if (arg == "--skip-hash") options.run_hash = false;
        else if (arg == "--skip-ac") options.run_ac = false;
        else if (arg == "--skip-accombo") options.run_combo = false;
        else if (arg == "--skip-ac-on-combo") options.run_ac_on_combo = false;
        else if (arg == "--skip-dynamic-delete") options.run_dynamic_delete = false;
        else { std::cerr << "未知选项：" << arg << '\n'; return 1; }
    }
    if (!data_explicit) options.data_root = options.work_root + "/data";
    if (!locate_source(options)) {
        std::cerr << "无法定位 Megascan/test/demo/920hg。请设置 MEGASCAN_ROOT。\n";
        return 1;
    }
    if (options.action == "summarize" && options.out_dir.empty()) {
        std::cerr << "summarize 必须指定 --out-dir。\n";
        return 1;
    }
    if ((options.action == "single" || options.action == "full" ||
         options.action == "quick" || options.action == "profile") &&
        options.machine != "920" && options.machine != "hg") {
        std::cerr << "该操作必须指定 --machine 920 或 --machine hg。\n";
        return 1;
    }
    if (!options.algorithm.empty()) {
        const std::set<std::string> supported = {
            "hash", "ac", "accombo", "ac_on_combo",
            "ac_delete", "accombo_delete", "accombo_combination_delete"
        };
        if (options.action != "quick" || supported.count(options.algorithm) == 0) {
            std::cerr << "--algorithm 仅用于 quick，且必须指定受支持的算法。\n";
            return 1;
        }
    }
    if (options.out_dir.empty() && !options.machine.empty()) {
        std::string kind = options.action == "full" ? "full" : options.action;
        options.out_dir = options.work_root + "/results/" + kind + "/" + options.machine;
    }
    if (options.action == "compare-single" || options.action == "compare-full") {
        std::string kind = options.action == "compare-full" ? "full" : "single";
        if (options.left_result.empty()) options.left_result = options.work_root + "/results/" + kind + "/920/summary.csv";
        if (options.right_result.empty()) options.right_result = options.work_root + "/results/" + kind + "/hg/summary.csv";
        if (options.left_env.empty()) options.left_env = options.work_root + "/results/" + kind + "/920/env.log";
        if (options.right_env.empty()) options.right_env = options.work_root + "/results/" + kind + "/hg/env.log";
        if (options.compare_output.empty()) options.compare_output = options.work_root + "/results/" + kind + "/920_vs_hg.csv";
    }
    if (options.trials == 0 || options.trials % 2 == 0) {
        std::cerr << "trials 必须是大于等于 1 的奇数。\n";
        return 1;
    }
    return 0;
}

} // namespace

extern "C" int demo_run_920hg_workflow(int argc, char **argv)
{
    Options options;
    int parsed = parse_options(argc, argv, options);
    if (parsed != 0) return parsed == 2 ? 0 : 1;
    if (options.action == "generate") return generate_matrix(options);
    if (options.action == "single") return benchmark_matrix(options, false);
    if (options.action == "full") return benchmark_matrix(options, true);
    if (options.action == "quick") return benchmark_matrix(options, false, true);
    if (options.action == "profile") return profile_matrix(options);
    if (options.action == "summarize") return demo_920hg_summarize(options.out_dir);
    if (options.action == "compare-single" || options.action == "compare-full") {
        mkdir_p(dirname_of(options.compare_output));
        return demo_920hg_compare(options.left_result, options.right_result,
                                  options.left_env, options.right_env,
                                  options.compare_output, options.min_speedup);
    }
    std::cerr << "未知的 920hg 操作：" << options.action << '\n';
    return 1;
}

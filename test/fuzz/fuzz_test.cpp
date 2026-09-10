#include "fuzz_test.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

extern "C" {
#include "ms_allocator.h"
#include "ms_common.h"
}

namespace {

const uint64_t DEFAULT_SEED = UINT64_C(0x4d6567617363616e);

class WorkQueue {
public:
    explicit WorkQueue(std::size_t capacity) : capacity_(capacity) {}

    bool push(FuzzTestCase testCase) {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [this] {
            return queue_.size() < capacity_ || closed_ || cancelled_;
        });
        if (closed_ || cancelled_) {
            return false;
        }
        queue_.push_back(std::move(testCase));
        notEmpty_.notify_one();
        return true;
    }

    bool pop(FuzzTestCase &testCase) {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock,
                       [this] { return !queue_.empty() || closed_ || cancelled_; });
        if (cancelled_ || queue_.empty()) {
            return false;
        }
        testCase = std::move(queue_.front());
        queue_.pop_front();
        notFull_.notify_one();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

private:
    const std::size_t capacity_;
    std::deque<FuzzTestCase> queue_;
    bool closed_ = false;
    bool cancelled_ = false;
    std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
};

void mergeStats(FuzzStats &destination, const FuzzStats &source) {
    destination.cases += source.cases;
    for (const auto &entry : source.calls) {
        destination.calls[entry.first] += entry.second;
    }
    for (const auto &entry : source.invocations) {
        destination.invocations[entry.first] += entry.second;
    }
    for (const auto &entry : source.failures) {
        destination.failures[entry.first] += entry.second;
    }
}

const char *const EXPORTED_APIS[] = {
    "ms_add_exprs", "ms_add_rule", "ms_alloc_scratch",
    "ms_close_handler_stream", "ms_close_stream",
    "ms_compile_database_multi", "ms_compile_database_multi_ext",
    "ms_compile_eval_combinations", "ms_compile_multi",
    "ms_compile_multi_ext", "ms_compile_scan_ac_combinations",
    "ms_create_database", "ms_find_database_rule", "ms_find_rule",
    "ms_flush_stream", "ms_free_database", "ms_free_scratch",
    "ms_get_match_events", "ms_handler_destroy", "ms_handler_init",
    "ms_logical_clear", "ms_logical_eval", "ms_logical_eval_inplace",
    "ms_logical_get", "ms_logical_parse", "ms_logical_set",
    "ms_logical_tree_destroy", "ms_open_handler_stream", "ms_open_stream",
    "ms_remove_database_rule", "ms_remove_rule", "ms_reset_stream",
    "ms_scan_database", "ms_scan_expr", "ms_scan_expr_stream",
    "ms_scan_expr_stream_flush", "ms_scan_rule_stream", "ms_scan_stream",
    "ms_set_allocator", "ms_set_database_allocator", "ms_set_hash_func",
    "ms_set_misc_allocator", "ms_set_scratch_allocator",
    "ms_set_stream_allocator"
};

const std::size_t EXPORTED_API_COUNT =
    sizeof(EXPORTED_APIS) / sizeof(EXPORTED_APIS[0]);

uint64_t callCount(const FuzzStats &stats, const char *api) {
    const auto entry = stats.calls.find(api);
    return entry == stats.calls.end() ? 0 : entry->second;
}

uint64_t invocationCount(const FuzzStats &stats, const char *api) {
    const auto entry = stats.invocations.find(api);
    return entry == stats.invocations.end() ? 0 : entry->second;
}

uint64_t minimumApiCalls(const FuzzStats &stats, std::string &api) {
    uint64_t minimum = UINT64_MAX;
    for (std::size_t i = 0; i < EXPORTED_API_COUNT; ++i) {
        const uint64_t count = callCount(stats, EXPORTED_APIS[i]);
        if (count < minimum) {
            minimum = count;
            api = EXPORTED_APIS[i];
        }
    }
    return minimum;
}

void printStats(const FuzzStats &stats, double seconds) {
    std::cout << "API call summary (exported=" << EXPORTED_API_COUNT << "):\n";
    for (std::size_t i = 0; i < EXPORTED_API_COUNT; ++i) {
        const char *api = EXPORTED_APIS[i];
        const uint64_t calls = callCount(stats, api);
        const uint64_t invocations = invocationCount(stats, api);
        const auto failure = stats.failures.find(api);
        const uint64_t failures = failure == stats.failures.end()
                                      ? 0 : failure->second;
        const double rate = seconds > 0.0 ? calls / seconds : 0.0;
        std::cout << "  " << std::left << std::setw(36) << api
                  << " calls=" << std::right << std::setw(12) << calls
                  << " helpers=" << std::setw(12)
                  << (invocations >= calls ? invocations - calls : 0)
                  << " failures=" << std::setw(12) << failures
                  << " rate=" << std::fixed << std::setprecision(2)
                  << rate << "/s\n";
    }
    if (!stats.failures.empty()) {
        std::cout << "Finding categories:\n";
        for (const auto &entry : stats.failures) {
            std::cout << "  " << entry.first << '=' << entry.second << '\n';
        }
    }
}

bool resetPublicAllocators(FuzzStats &stats, std::string &error) {
    struct Result {
        const char *name;
        int value;
    };
    const Result results[] = {
        {"ms_set_allocator", ms_set_allocator(nullptr, nullptr, nullptr)},
        {"ms_set_database_allocator",
         ms_set_database_allocator(nullptr, nullptr, nullptr)},
        {"ms_set_scratch_allocator",
         ms_set_scratch_allocator(nullptr, nullptr, nullptr)},
        {"ms_set_stream_allocator",
         ms_set_stream_allocator(nullptr, nullptr, nullptr)},
        {"ms_set_misc_allocator",
         ms_set_misc_allocator(nullptr, nullptr, nullptr)}
    };

    for (const auto &result : results) {
        stats.invocations[result.name]++;
        if (result.value != MS_SUCCESS) {
            error = std::string(result.name) + " expected=0 actual=" +
                    std::to_string(result.value);
            return false;
        }
    }
    return true;
}

} // namespace

int runFuzz(const FuzzConfig &config) {
    FuzzConfig effective = config;
    if (effective.seed == 0) {
        effective.seed = DEFAULT_SEED;
    }

    const auto started = std::chrono::steady_clock::now();
    std::cout << "Megascan local fuzz: seed=" << effective.seed
              << " rounds=" << effective.rounds
              << " threads=" << effective.threads;
    if (effective.durationSeconds != 0) {
        std::cout << " duration_seconds=" << effective.durationSeconds;
    }
    std::cout << '\n';

    std::unique_ptr<Generator> generator = createGenerator(effective);
    WorkQueue queue(effective.queueSize);
    std::atomic<bool> abortRequested(false);
    std::atomic<uint64_t> failureCount(0);
    std::mutex resultMutex;
    std::string firstError;
    uint32_t failedRound = 0;
    FuzzStats combinedStats;
    std::string setupError;
    if (!resetPublicAllocators(combinedStats, setupError)) {
        std::cerr << "FAIL " << setupError << '\n';
        return 1;
    }

    if (effective.callsPerApi != 0) {
        std::unique_ptr<Runner> runner = createRunner();
        uint64_t findingCases = 0;
        std::string firstError;
        uint32_t generated = 0;
        auto nextProgress = started + std::chrono::minutes(1);
        const auto deadline = started +
            std::chrono::seconds(effective.durationSeconds);

        while (generated < effective.rounds) {
            if (effective.durationSeconds != 0 &&
                std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            FuzzTestCase testCase = generator->generate(generated);
            FuzzStats before = combinedStats;
            mergeStats(before, runner->stats());
            // Round-robin among unfinished APIs. A fast API therefore cannot
            // run far ahead while a slower API remains untested.
            if (effective.callsPerApi != 0) {
                for (std::size_t offset = 0; offset < EXPORTED_API_COUNT; ++offset) {
                    const std::size_t index =
                        (static_cast<std::size_t>(generated) + offset) %
                        EXPORTED_API_COUNT;
                    if (callCount(before, EXPORTED_APIS[index]) <
                        effective.callsPerApi) {
                        testCase.targetApi = EXPORTED_APIS[index];
                        break;
                    }
                }
            } else {
                testCase.targetApi =
                    EXPORTED_APIS[generated % EXPORTED_API_COUNT];
            }
            if (effective.callsPerApi != 0 && testCase.targetApi.empty()) {
                break;
            }
            std::string error;
            if (!runner->run(testCase, error)) {
                ++findingCases;
                if (firstError.empty()) {
                    firstError = error;
                }
            }
            ++generated;

            if ((generated & 1023u) == 0u) {
                FuzzStats snapshot = combinedStats;
                mergeStats(snapshot, runner->stats());
                std::string slowestApi;
                const uint64_t minimum = minimumApiCalls(snapshot, slowestApi);
                if (effective.callsPerApi != 0 &&
                    minimum >= effective.callsPerApi) {
                    break;
                }
                const auto now = std::chrono::steady_clock::now();
                if (now >= nextProgress) {
                    const double seconds =
                        std::chrono::duration<double>(now - started).count();
                    const double rate = seconds > 0.0 ? minimum / seconds : 0.0;
                    const double eta = rate > 0.0
                        ? (effective.callsPerApi - minimum) / rate : 0.0;
                    std::cout << "PROGRESS cases=" << generated
                              << " slowest=" << slowestApi
                              << " calls=" << minimum << '/'
                              << effective.callsPerApi
                              << " rate=" << std::fixed << std::setprecision(2)
                              << rate << "/s eta_seconds=" << eta << '\n';
                    nextProgress = now + std::chrono::minutes(1);
                }
            }
        }

        mergeStats(combinedStats, runner->stats());
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        std::string slowestApi;
        const uint64_t minimum = minimumApiCalls(combinedStats, slowestApi);
        if (findingCases != 0) {
            std::cerr << "FINDINGS count=" << findingCases
                      << " first=" << firstError << '\n';
        }
        std::cout << "COMPLETE cases=" << generated
                  << " minimum_api=" << slowestApi
                  << " minimum_calls=" << minimum
                  << " target=" << effective.callsPerApi << '\n';
        printStats(combinedStats, seconds);
        const bool targetReached = effective.callsPerApi == 0 ||
                                   minimum >= effective.callsPerApi;
        const bool timedBenchmark = effective.durationSeconds != 0;
        return (targetReached || timedBenchmark) &&
                       (effective.reportOnly || findingCases == 0)
                   ? 0
                   : 1;
    }

    std::vector<std::thread> workers;
    workers.reserve(effective.threads);

    for (uint32_t i = 0; i < effective.threads; ++i) {
        workers.emplace_back([&] {
            std::unique_ptr<Runner> runner = createRunner();
            FuzzTestCase testCase;
            while (queue.pop(testCase)) {
                std::string error;
                if (!runner->run(testCase, error)) {
                    const uint64_t previous = failureCount.fetch_add(1);
                    if (previous == 0) {
                        std::lock_guard<std::mutex> lock(resultMutex);
                        failedRound = testCase.round;
                        firstError = error;
                    }
                    if (!effective.reportOnly) {
                        abortRequested.store(true);
                        queue.cancel();
                        break;
                    }
                    continue;
                }
                if (effective.verbose) {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    std::cout << "PASS round=" << testCase.round << '\n';
                }
            }
            std::lock_guard<std::mutex> lock(resultMutex);
            mergeStats(combinedStats, runner->stats());
        });
    }

    uint32_t generated = 0;
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(effective.durationSeconds);
    for (; generated < effective.rounds && !abortRequested.load(); ++generated) {
        if (effective.durationSeconds != 0 &&
            std::chrono::steady_clock::now() >= deadline) {
            break;
        }
        FuzzTestCase testCase = generator->generate(generated);
        if (effective.durationSeconds != 0) {
            testCase.targetApi = EXPORTED_APIS[generated % EXPORTED_API_COUNT];
        }
        if (!queue.push(std::move(testCase))) {
            break;
        }
    }
    queue.close();
    for (auto &worker : workers) {
        worker.join();
    }

    const uint64_t failures = failureCount.load();
    if (failures != 0) {
        std::cerr << (effective.reportOnly ? "FINDINGS " : "FAIL ")
                  << "count=" << failures << " first=" << firstError << '\n'
                  << "Replay: megascan_fuzz --seed " << effective.seed
                  << " --rounds " << (static_cast<uint64_t>(failedRound) + 1u)
                  << " --threads 1 --verbose\n";
        if (!effective.reportOnly) {
            return 1;
        }
    }

    std::cout << (failures == 0 ? "PASS" : "COMPLETE")
              << " cases=" << combinedStats.cases
              << " generated=" << generated << '\n';
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    if (effective.durationSeconds != 0) {
        std::string slowestApi;
        const uint64_t minimum = minimumApiCalls(combinedStats, slowestApi);
        std::cout << "minimum_api=" << slowestApi
                  << " minimum_calls=" << minimum << '\n';
    }
    printStats(combinedStats, seconds);
    const bool durationCompleted = effective.durationSeconds != 0 &&
                                   generated < effective.rounds;
    return durationCompleted || combinedStats.cases == effective.rounds ? 0 : 1;
}

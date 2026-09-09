#include "fuzz_test.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
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
        stats.calls[result.name]++;
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
        if (!queue.push(generator->generate(generated))) {
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
    if (effective.verbose) {
        std::cout << "API calls:\n";
        for (const auto &entry : combinedStats.calls) {
            std::cout << "  " << entry.first << '=' << entry.second << '\n';
        }
    }
    const bool durationCompleted = effective.durationSeconds != 0 &&
                                   generated < effective.rounds;
    return durationCompleted || combinedStats.cases == effective.rounds ? 0 : 1;
}

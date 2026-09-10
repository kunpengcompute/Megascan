#ifndef MEGASCAN_FUZZ_TEST_H
#define MEGASCAN_FUZZ_TEST_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct FuzzTestCase {
    std::vector<std::string> expressions;
    std::vector<uint32_t> ids;
    std::vector<uint32_t> flags;
    std::vector<uint8_t> data;
    uint64_t seed = 0;
    uint32_t round = 0;
    bool exerciseAllocatorApis = false;
    // Empty means run the complete coverage workflow. In calls-per-api mode
    // this is the one exported API being measured; other public calls are
    // setup/cleanup helpers and are accounted separately.
    std::string targetApi;
};

struct FuzzConfig {
    uint64_t seed = 0;
    uint32_t rounds = 1000;
    uint32_t durationSeconds = 0;
    uint64_t callsPerApi = 0;
    uint32_t maxPatternLength = 16;
    uint32_t maxDataLength = 256;
    uint32_t threads = 1;
    std::size_t queueSize = 256;
    bool verbose = false;
    bool reportOnly = false;
};

struct FuzzStats {
    uint64_t cases = 0;
    // Calls made while the named API was the explicit fuzz target.
    std::map<std::string, uint64_t> calls;
    // Every real exported call, including setup and cleanup dependencies.
    std::map<std::string, uint64_t> invocations;
    std::map<std::string, uint64_t> failures;
};

class Generator {
public:
    virtual ~Generator() = default;
    virtual FuzzTestCase generate(uint32_t round) = 0;
};

class Runner {
public:
    virtual ~Runner() = default;
    virtual bool run(const FuzzTestCase &testCase, std::string &error) = 0;
    virtual const FuzzStats &stats() const = 0;
};

std::unique_ptr<Generator> createGenerator(const FuzzConfig &config);
std::unique_ptr<Runner> createRunner();
int runFuzz(const FuzzConfig &config);

#endif

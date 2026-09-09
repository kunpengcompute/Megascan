#include "fuzz_test.h"
#include "data/data_generator.h"

extern "C" {
#include "ms_common.h"
}

#include <cstdio>
#include <cstdlib>

namespace {

class MsGenerator final : public Generator {
public:
    explicit MsGenerator(const FuzzConfig &config)
        : config_(config), state_(config.seed ? config.seed : UINT64_C(0x4d6567617363616e)) {}

    FuzzTestCase generate(uint32_t round) override {
        static const char alphabet[] = "abcdef0123456789";
        FuzzTestCase testCase;
        const uint32_t plainCount = 2u + range(7u);

        testCase.seed = config_.seed;
        testCase.round = round;
        testCase.expressions.reserve(plainCount + 1u);
        testCase.ids.reserve(plainCount + 1u);
        testCase.flags.reserve(plainCount + 1u);

        for (uint32_t i = 0; i < plainCount; i++) {
            std::string pattern = "r" + std::to_string(i) + "_";
            const uint32_t randomLength = 1u + range(config_.maxPatternLength);
            for (uint32_t j = 0; j < randomLength; j++) {
                pattern.push_back(alphabet[range(sizeof(alphabet) - 1u)]);
            }
            testCase.expressions.push_back(pattern);
            testCase.ids.push_back(100u + i);
            testCase.flags.push_back(range(4u) == 0u ? MS_FLAG_QUIET : MS_FLAG_NONE);
        }

        testCase.expressions.push_back(std::to_string(testCase.ids[0]) + "&" +
                                       std::to_string(testCase.ids[1]));
        testCase.ids.push_back(10000u + round);
        testCase.flags.push_back(MS_FLAG_COMBINATION);
        testCase.data = generateTestData(testCase);
        return testCase;
    }

private:
    uint64_t next() {
        uint64_t x = state_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state_ = x;
        return x * UINT64_C(2685821657736338717);
    }

    uint32_t range(uint32_t limit) {
        return limit ? static_cast<uint32_t>(next() % limit) : 0;
    }

    std::vector<uint8_t> generateTestData(const FuzzTestCase &testCase) {
        return generateData(testCase.expressions);
    }

    std::vector<uint8_t> generateData(const std::vector<std::string> &expressions) {
        std::vector<std::string> plain(expressions.begin(), expressions.end() - 1);
        return generateTestDataImpl(plain);
    }

    std::vector<uint8_t> generateTestDataImpl(const std::vector<std::string> &plain) {
        return ::generateTestData(state_, plain, config_.maxDataLength);
    }

    FuzzConfig config_;
    uint64_t state_;
};

} // namespace

std::unique_ptr<Generator> createGenerator(const FuzzConfig &config) {
    return std::unique_ptr<Generator>(new MsGenerator(config));
}

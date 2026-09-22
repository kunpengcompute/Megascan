#include "data_generator.h"

#include <cstring>

namespace {

uint64_t nextRandom(uint64_t &state) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * UINT64_C(2685821657736338717);
}

uint32_t randomRange(uint64_t &state, uint32_t limit) {
    return limit ? static_cast<uint32_t>(nextRandom(state) % limit) : 0;
}

} // namespace

std::vector<uint8_t> generateTestData(uint64_t &state,
                                      const std::vector<std::string> &patterns,
                                      uint32_t maxLength) {
    const uint32_t length = 1 + randomRange(state, maxLength);
    std::vector<uint8_t> data(length);

    for (auto &value : data) {
        value = static_cast<uint8_t>(randomRange(state, 256));
    }
    for (const auto &pattern : patterns) {
        if (pattern.size() <= data.size() && randomRange(state, 2)) {
            const uint32_t room =
                static_cast<uint32_t>(data.size() - pattern.size() + 1);
            const uint32_t offset = randomRange(state, room);
            std::memcpy(data.data() + offset, pattern.data(), pattern.size());
        }
    }
    return data;
}

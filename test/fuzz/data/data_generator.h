#ifndef MEGASCAN_FUZZ_DATA_GENERATOR_H
#define MEGASCAN_FUZZ_DATA_GENERATOR_H

#include <cstdint>
#include <string>
#include <vector>

std::vector<uint8_t> generateTestData(uint64_t &state,
                                      const std::vector<std::string> &patterns,
                                      uint32_t maxLength);

#endif

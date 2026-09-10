#include "fuzz_test.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

void printUsage(const char *program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --seed N                 Random seed (0 uses the built-in seed)\n"
        << "  --rounds N               Number of cases (default: 1000)\n"
        << "  --duration-seconds N     Stop normally after approximately N seconds\n"
        << "  --calls-per-api N        Run until every exported API reaches N calls\n"
        << "  --max-pattern-length N   Maximum random pattern suffix (default: 16)\n"
        << "  --max-data-length N      Maximum input length (default: 256)\n"
        << "  --threads N              Local worker threads (default: 1)\n"
        << "  --queue-size N           In-process queue capacity (default: 256)\n"
        << "  --report-only            Record findings and finish all coverage rounds\n"
        << "  -v, --verbose            Print every completed case\n"
        << "  -h, --help               Show this help\n";
}

bool parseUnsigned(const char *text, uint64_t maximum, uint64_t &value) {
    if (text == nullptr || *text == '\0' || *text == '-') {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 0);
    if (errno == ERANGE || end == text || *end != '\0' || parsed > maximum) {
        return false;
    }
    value = static_cast<uint64_t>(parsed);
    return true;
}

bool readValue(int argc, char **argv, int &index, uint64_t maximum,
               uint64_t &value) {
    if (index + 1 >= argc) {
        std::cerr << "missing value for " << argv[index] << '\n';
        return false;
    }
    ++index;
    if (!parseUnsigned(argv[index], maximum, value)) {
        std::cerr << "invalid value for " << argv[index - 1] << ": "
                  << argv[index] << '\n';
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    FuzzConfig config;
    const uint64_t uint32Max = std::numeric_limits<uint32_t>::max();
    bool roundsSpecified = false;

    for (int i = 1; i < argc; ++i) {
        const std::string option(argv[i]);
        uint64_t value = 0;

        if (option == "-h" || option == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (option == "-v" || option == "--verbose") {
            config.verbose = true;
            continue;
        }
        if (option == "--report-only") {
            config.reportOnly = true;
            continue;
        }
        if (option == "--seed") {
            if (!readValue(argc, argv, i,
                           std::numeric_limits<uint64_t>::max(), value)) {
                return 2;
            }
            config.seed = value;
        } else if (option == "--rounds") {
            if (!readValue(argc, argv, i, uint32Max, value)) {
                return 2;
            }
            config.rounds = static_cast<uint32_t>(value);
            roundsSpecified = true;
        } else if (option == "--duration-seconds") {
            if (!readValue(argc, argv, i, uint32Max, value)) {
                return 2;
            }
            config.durationSeconds = static_cast<uint32_t>(value);
        } else if (option == "--calls-per-api") {
            if (!readValue(argc, argv, i,
                           std::numeric_limits<uint64_t>::max(), value)) {
                return 2;
            }
            config.callsPerApi = value;
        } else if (option == "--max-pattern-length") {
            if (!readValue(argc, argv, i, uint32Max, value)) {
                return 2;
            }
            config.maxPatternLength = static_cast<uint32_t>(value);
        } else if (option == "--max-data-length") {
            if (!readValue(argc, argv, i, uint32Max, value)) {
                return 2;
            }
            config.maxDataLength = static_cast<uint32_t>(value);
        } else if (option == "--threads") {
            if (!readValue(argc, argv, i, uint32Max, value)) {
                return 2;
            }
            config.threads = static_cast<uint32_t>(value);
        } else if (option == "--queue-size") {
            if (!readValue(argc, argv, i,
                           std::numeric_limits<std::size_t>::max(), value)) {
                return 2;
            }
            config.queueSize = static_cast<std::size_t>(value);
        } else {
            std::cerr << "unknown option: " << option << '\n';
            printUsage(argv[0]);
            return 2;
        }
    }

    if (config.rounds == 0 || config.maxPatternLength == 0 ||
        config.maxDataLength == 0 || config.threads == 0 ||
        config.queueSize == 0) {
        std::cerr << "rounds, lengths, threads, and queue size must be greater "
                     "than zero\n";
        return 2;
    }
    if (config.durationSeconds != 0 && !roundsSpecified) {
        config.rounds = std::numeric_limits<uint32_t>::max();
    }
    if (config.callsPerApi != 0) {
        if (config.threads != 1) {
            std::cerr << "calls-per-api scheduling requires --threads 1 so the "
                         "minimum API call target remains exact\n";
            return 2;
        }
        config.rounds = std::numeric_limits<uint32_t>::max();
    }
    return runFuzz(config);
}

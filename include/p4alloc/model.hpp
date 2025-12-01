#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace p4alloc {

using Memory = std::int64_t;

struct Error {
    std::string message;
};

template <typename T>
using Result = std::variant<T, Error>;

enum class MemoryType { sram, tcam };

struct Pipeline {
    std::size_t stages;
    Memory sram_per_stage;
    Memory tcam_per_stage;
};

struct Table {
    std::string name;
    MemoryType type;
    Memory size;
    std::vector<std::string> match_deps;
    std::vector<std::string> action_deps;
};

struct Program {
    Pipeline pipeline;
    std::vector<Table> tables;
};

}

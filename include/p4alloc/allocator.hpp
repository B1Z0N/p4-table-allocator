#pragma once

#include "p4alloc/model.hpp"
#include <concepts>
#include <string_view>

namespace p4alloc {

struct Fragment {
    std::size_t table;
    Memory size;
};

struct Stage {
    Memory sram_used = 0;
    Memory tcam_used = 0;
    std::vector<Fragment> tables;
};

struct Allocation {
    std::string algorithm;
    bool fits = false;
    std::vector<Stage> stages;
    std::vector<std::optional<std::size_t>> first_stage;
    std::vector<std::optional<std::size_t>> last_stage;
    std::optional<std::string> failure;

    std::size_t stages_used() const;
};

struct GreedyAllocator {
    Result<Allocation> operator()(const Program& program) const;
};

struct ConstrainedAllocator {
    Result<Allocation> operator()(const Program& program) const;
};

template <typename T>
concept Allocator = requires(const T allocator, const Program& program) {
    { allocator(program) } -> std::same_as<Result<Allocation>>;
};

template <Allocator T>
Result<Allocation> allocate(const Program& program, const T& allocator) {
    return allocator(program);
}

static_assert(Allocator<GreedyAllocator>);
static_assert(Allocator<ConstrainedAllocator>);

}

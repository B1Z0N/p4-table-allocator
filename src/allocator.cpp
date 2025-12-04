#include "p4alloc/allocator.hpp"
#include "p4alloc/graph.hpp"
#include <algorithm>
#include <span>

namespace p4alloc {
namespace {

Memory capacity(const Pipeline& pipeline, MemoryType type) {
    return type == MemoryType::sram ? pipeline.sram_per_stage : pipeline.tcam_per_stage;
}

Memory used(const Stage& stage, MemoryType type) {
    return type == MemoryType::sram ? stage.sram_used : stage.tcam_used;
}

struct Chunk {
    std::size_t stage;
    Memory size;
};

std::optional<std::vector<Chunk>> find_placement(const Table& table, const Pipeline& pipeline,
                                                std::span<const Stage> stages, std::size_t earliest) {
    const auto budget = capacity(pipeline, table.type);
    if (table.size <= budget) {
        for (std::size_t start = earliest; start < stages.size(); ++start) {
            if (table.size <= budget - used(stages[start], table.type)) {
                return std::vector<Chunk>{{start, table.size}};
            }
        }
        return std::nullopt;
    }
    if (budget == 0) {
        return std::nullopt;
    }
    std::vector<Chunk> chunks;
    auto remaining = table.size;
    for (std::size_t stage = earliest; stage < stages.size(); ++stage) {
        const auto free = budget - used(stages[stage], table.type);
        if (free == 0) {
            chunks.clear();
            remaining = table.size;
            continue;
        }
        const auto amount = std::min(free, remaining);
        chunks.push_back({stage, amount});
        remaining -= amount;
        if (remaining == 0) {
            return chunks;
        }
    }
    return std::nullopt;
}

Allocation place_ordered(const Program& program, const Graph& graph,
                         std::span<const std::size_t> order, std::string_view algorithm) {
    Allocation result{std::string(algorithm), false, std::vector<Stage>(program.pipeline.stages),
                      std::vector<std::optional<std::size_t>>(program.tables.size()),
                      std::vector<std::optional<std::size_t>>(program.tables.size()), std::nullopt};
    for (const auto index : order) {
        const auto& table = program.tables[index];
        std::size_t earliest = 0;
        for (const auto& edge : graph.incoming[index]) {
            earliest = std::max(earliest, result.last_stage[edge.table].value() + edge.gap);
        }
        const auto chunks = find_placement(table, program.pipeline, result.stages, earliest);
        if (!chunks) {
            result.failure = "table '" + table.name + "' cannot fit at or after stage " +
                             std::to_string(earliest) + " within memory and consecutive-stage constraints";
            return result;
        }
        for (const auto& chunk : *chunks) {
            auto& stage = result.stages[chunk.stage];
            auto& memory = table.type == MemoryType::sram ? stage.sram_used : stage.tcam_used;
            memory += chunk.size;
            stage.tables.push_back({index, chunk.size});
        }
        result.first_stage[index] = chunks->front().stage;
        result.last_stage[index] = chunks->back().stage;
    }
    result.fits = true;
    return result;
}

Result<Allocation> run(const Program& program) {
    const auto& p = program.pipeline;
    if (p.stages == 0 || p.stages > 1000000 || p.sram_per_stage < 0 || p.tcam_per_stage < 0) {
        return Error{"invalid pipeline: require 1..1000000 stages and nonnegative capacities"};
    }
    for (const auto& table : program.tables) {
        if (table.size < 0) {
            return Error{"table '" + table.name + "': size must not be negative"};
        }
    }
    const auto graph_result = build_graph(program.tables);
    if (const auto* error = std::get_if<Error>(&graph_result)) {
        return *error;
    }
    const auto& graph = std::get<Graph>(graph_result);
    return place_ordered(program, graph, graph.order, "greedy");
}

}

std::size_t Allocation::stages_used() const {
    std::size_t count = 0;
    for (const auto& last : last_stage) {
        if (last) {
            count = std::max(count, *last + 1);
        }
    }
    return count;
}

Result<Allocation> GreedyAllocator::operator()(const Program& program) const {
    return run(program);
}

}

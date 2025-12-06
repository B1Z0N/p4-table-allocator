#include "p4alloc/report.hpp"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

namespace p4alloc {
namespace {

std::string_view type_name(MemoryType type) {
    return type == MemoryType::sram ? "exact" : "tcam";
}

}

nlohmann::json json_report(const Program& program, const Allocation& allocation) {
    using nlohmann::json;
    json stages = json::array();
    for (std::size_t index = 0; index < allocation.stages.size(); ++index) {
        const auto& stage = allocation.stages[index];
        json fragments = json::array();
        for (const auto& fragment : stage.tables) {
            const auto& table = program.tables[fragment.table];
            fragments.push_back({{"name", table.name}, {"type", type_name(table.type)}, {"size", fragment.size}});
        }
        stages.push_back({{"stage", index}, {"sram_used", stage.sram_used}, {"tcam_used", stage.tcam_used},
                          {"tables", std::move(fragments)}});
    }
    json tables = json::array();
    for (std::size_t index = 0; index < program.tables.size(); ++index) {
        const auto& table = program.tables[index];
        const auto first = allocation.first_stage[index];
        const auto last = allocation.last_stage[index];
        tables.push_back({{"name", table.name}, {"type", type_name(table.type)}, {"size", table.size},
                          {"first_stage", first ? json(*first) : json(nullptr)},
                          {"last_stage", last ? json(*last) : json(nullptr)}});
    }
    const auto& p = program.pipeline;
    return {{"algorithm", allocation.algorithm}, {"fits", allocation.fits},
            {"stages_used", allocation.stages_used()},
            {"pipeline", {{"stages", p.stages}, {"sram_per_stage", p.sram_per_stage},
                          {"tcam_per_stage", p.tcam_per_stage}}},
            {"stages", std::move(stages)}, {"tables", std::move(tables)},
            {"error", allocation.failure ? json(*allocation.failure) : json(nullptr)}};
}

void print_report(std::ostream& stream, const Program& program, const Allocation& allocation) {
    stream << "Algorithm: " << allocation.algorithm << '\n';
    stream << std::left << std::setw(8) << "Stage" << std::setw(24) << "SRAM used/capacity"
           << std::setw(24) << "TCAM used/capacity" << "Tables (fragment size)\n";
    for (std::size_t index = 0; index < allocation.stages.size(); ++index) {
        const auto& stage = allocation.stages[index];
        const auto sram = std::to_string(stage.sram_used) + "/" + std::to_string(program.pipeline.sram_per_stage);
        const auto tcam = std::to_string(stage.tcam_used) + "/" + std::to_string(program.pipeline.tcam_per_stage);
        stream << std::setw(8) << index << std::setw(24) << sram << std::setw(24) << tcam;
        bool first = true;
        for (const auto& fragment : stage.tables) {
            stream << (first ? "" : ", ") << program.tables[fragment.table].name << '(' << fragment.size << ')';
            first = false;
        }
        stream << (first ? "-" : "") << '\n';
    }
    stream << (allocation.fits ? "fits" : "doesn't fit") << "; stages used: " << allocation.stages_used()
           << '/' << program.pipeline.stages << '\n';
    if (allocation.failure) {
        stream << "Partial placement: " << *allocation.failure << '\n';
    }
}

}

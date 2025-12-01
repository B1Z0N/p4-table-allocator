#include "p4alloc/input.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>

namespace p4alloc {
namespace {

using nlohmann::json;

void check_keys(const json& value, std::initializer_list<std::string_view> allowed,
                const std::string& context) {
    if (!value.is_object()) {
        throw std::runtime_error(context + " must be an object");
    }
    for (const auto& [key, unused] : value.items()) {
        static_cast<void>(unused);
        if (std::ranges::find(allowed, key) == allowed.end()) {
            throw std::runtime_error(context + ": unknown key '" + key + "'");
        }
    }
}

Memory nonnegative(const json& value, const std::string& context) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() && value.get<std::uint64_t>() >
             static_cast<std::uint64_t>(std::numeric_limits<Memory>::max()))) {
        throw std::runtime_error(context + " must be a nonnegative 64-bit integer");
    }
    const auto result = value.get<Memory>();
    if (result < 0) {
        throw std::runtime_error(context + " must not be negative");
    }
    return result;
}

std::string name_value(const json& value, const std::string& context) {
    if (!value.is_string()) {
        throw std::runtime_error(context + " must be a nonempty string");
    }
    auto name = value.get<std::string>();
    if (name.empty() || std::ranges::all_of(name, [](unsigned char c) { return std::isspace(c); })) {
        throw std::runtime_error(context + " must be a nonempty name");
    }
    if (std::ranges::any_of(name, [](unsigned char c) { return std::iscntrl(c); })) {
        throw std::runtime_error(context + " must not contain control characters");
    }
    return name;
}

std::vector<std::string> dependencies(const json& table, const std::string& key) {
    std::vector<std::string> result;
    if (!table.contains(key)) {
        return result;
    }
    if (!table.at(key).is_array()) {
        throw std::runtime_error(key + " must be an array of table names");
    }
    for (const auto& value : table.at(key)) {
        result.push_back(name_value(value, key));
    }
    return result;
}

}

Result<Program> parse_input(std::string_view input) {
    try {
        const auto root = json::parse(input);
        check_keys(root, {"pipeline", "tables"}, "input");
        const auto& pipeline = root.at("pipeline");
        check_keys(pipeline, {"stages", "sram_per_stage", "tcam_per_stage"}, "pipeline");
        const auto stages = nonnegative(pipeline.at("stages"), "pipeline.stages");
        if (stages == 0 || stages > 1000000) {
            return Error{"pipeline.stages must be between 1 and 1000000"};
        }
        Program program{{static_cast<std::size_t>(stages),
                         nonnegative(pipeline.at("sram_per_stage"), "pipeline.sram_per_stage"),
                         nonnegative(pipeline.at("tcam_per_stage"), "pipeline.tcam_per_stage")}, {}};
        const auto& tables = root.at("tables");
        if (!tables.is_array()) {
            return Error{"tables must be an array"};
        }
        std::set<std::string> names;
        for (const auto& table : tables) {
            check_keys(table, {"name", "type", "size", "match_deps", "action_deps"}, "table");
            auto name = name_value(table.at("name"), "table.name");
            if (!names.insert(name).second) {
                return Error{"duplicate table name '" + name + "'"};
            }
            const auto type = table.at("type").get<std::string>();
            if (type != "exact" && type != "tcam") {
                return Error{"table '" + name + "': type must be 'exact' or 'tcam'"};
            }
            const auto size = nonnegative(table.at("size"), "table '" + name + "'.size");
            program.tables.push_back({std::move(name), type == "exact" ? MemoryType::sram : MemoryType::tcam,
                                      size, dependencies(table, "match_deps"), dependencies(table, "action_deps")});
        }
        return program;
    } catch (const json::exception& e) {
        return Error{"invalid JSON input: " + std::string(e.what())};
    } catch (const std::runtime_error& e) {
        return Error{e.what()};
    }
}

Result<Program> load_input(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        return Error{"cannot open input file '" + path + "'"};
    }
    const std::string contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        return Error{"cannot read input file '" + path + "'"};
    }
    return parse_input(contents);
}

}

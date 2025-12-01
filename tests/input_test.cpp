#include "p4alloc/input.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <limits>

using namespace p4alloc;
using nlohmann::json;

static json valid_input() {
    return {{"pipeline", {{"stages", 4}, {"sram_per_stage", 100}, {"tcam_per_stage", 20}}},
            {"tables", {{{"name", "eth_check"}, {"type", "exact"}, {"size", 30}}}}};
}

TEST_CASE("parse a valid program with optional dependency lists") {
    const auto result = parse_input(valid_input().dump());
    REQUIRE(std::holds_alternative<Program>(result));
    const auto& p = std::get<Program>(result);
    REQUIRE(p.pipeline.stages == 4);
    REQUIRE(p.tables.size() == 1);
    CHECK(p.tables[0].type == MemoryType::sram);
    CHECK(p.tables[0].size == 30);
    CHECK(p.tables[0].match_deps.empty());
}

TEST_CASE("parse tcam tables and both dependency kinds") {
    auto data = valid_input();
    data["tables"].push_back({{"name", "acl"}, {"type", "tcam"}, {"size", 15},
                               {"match_deps", {"eth_check"}}, {"action_deps", {"eth_check"}}});
    const auto result = parse_input(data.dump());
    REQUIRE(std::holds_alternative<Program>(result));
    CHECK(std::get<Program>(result).tables[1].type == MemoryType::tcam);
    CHECK(std::get<Program>(result).tables[1].match_deps == std::vector<std::string>{"eth_check"});
}

TEST_CASE("reject negative table sizes with context") {
    auto data = valid_input();
    data["tables"][0]["size"] = -1;
    const auto result = parse_input(data.dump());
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).message.find("size") != std::string::npos);
}

TEST_CASE("reject invalid schema values") {
    auto data = valid_input();
    SECTION("fractional size") { data["tables"][0]["size"] = 1.5; }
    SECTION("boolean size") { data["tables"][0]["size"] = true; }
    SECTION("oversized integer") { data["tables"][0]["size"] = std::numeric_limits<std::uint64_t>::max(); }
    SECTION("missing size") { data["tables"][0].erase("size"); }
    SECTION("unknown type") { data["tables"][0]["type"] = "wildcard"; }
    SECTION("empty name") { data["tables"][0]["name"] = ""; }
    SECTION("whitespace name") { data["tables"][0]["name"] = " \t"; }
    SECTION("duplicate name") { data["tables"].push_back(data["tables"][0]); }
    SECTION("dependencies must be arrays") { data["tables"][0]["match_deps"] = "eth_check"; }
    SECTION("dependencies must be names") { data["tables"][0]["action_deps"] = {5}; }
    SECTION("zero stages") { data["pipeline"]["stages"] = 0; }
    SECTION("negative stages") { data["pipeline"]["stages"] = -1; }
    SECTION("fractional stages") { data["pipeline"]["stages"] = 2.5; }
    SECTION("huge stages") { data["pipeline"]["stages"] = 1000001; }
    SECTION("negative capacity") { data["pipeline"]["sram_per_stage"] = -1; }
    SECTION("null tables") { data["tables"] = nullptr; }
    SECTION("unknown root key") { data["pipline"] = data["pipeline"]; }
    SECTION("unknown pipeline key") { data["pipeline"]["sram"] = 50; }
    SECTION("unknown table key") { data["tables"][0]["match_dep"] = json::array(); }
    CHECK(std::holds_alternative<Error>(parse_input(data.dump())));
}

TEST_CASE("malformed JSON and missing files are errors") {
    CHECK(std::holds_alternative<Error>(parse_input("{bad")));
    CHECK(std::holds_alternative<Error>(parse_input("[]")));
    CHECK(std::holds_alternative<Error>(load_input("file-that-does-not-exist.json")));
}

TEST_CASE("empty programs and zero sizes and capacities are valid") {
    auto data = valid_input();
    data["pipeline"]["sram_per_stage"] = 0;
    data["pipeline"]["tcam_per_stage"] = 0;
    data["tables"][0]["size"] = 0;
    CHECK(std::holds_alternative<Program>(parse_input(data.dump())));
    data["tables"] = json::array();
    CHECK(std::holds_alternative<Program>(parse_input(data.dump())));
}

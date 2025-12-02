#include "p4alloc/graph.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace p4alloc;

TEST_CASE("topological order respects both kinds of dependencies") {
    const std::vector<Table> tables{
        {"acl", MemoryType::tcam, 10, {}, {"route"}},
        {"route", MemoryType::tcam, 15, {"eth"}, {}},
        {"eth", MemoryType::sram, 30, {}, {}}};
    const auto result = build_graph(tables);
    REQUIRE(std::holds_alternative<Graph>(result));
    const auto& graph = std::get<Graph>(result);
    CHECK(graph.order == std::vector<std::size_t>{2, 1, 0});
    CHECK(graph.incoming[1][0].gap == 1);
    CHECK(graph.incoming[0][0].gap == 0);
    CHECK(graph.critical_path[2] == 1);
}

TEST_CASE("independent ready tables use deterministic input order") {
    const std::vector<Table> tables{
        {"a", MemoryType::sram, 1, {}, {}},
        {"b", MemoryType::sram, 1, {}, {}},
        {"c", MemoryType::sram, 1, {}, {}}};
    const auto result = build_graph(tables);
    REQUIRE(std::holds_alternative<Graph>(result));
    CHECK(std::get<Graph>(result).order == std::vector<std::size_t>{0, 1, 2});
}

TEST_CASE("unknown dependencies name the missing table") {
    const std::vector<Table> tables{{"a", MemoryType::sram, 1, {"missing"}, {}}};
    const auto result = build_graph(tables);
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).message.find("missing") != std::string::npos);
}

TEST_CASE("cycles in match action mixed and self edges are rejected") {
    std::vector<Table> tables{
        {"a", MemoryType::sram, 1, {}, {}}, {"b", MemoryType::sram, 1, {}, {}}};
    SECTION("match") { tables[0].match_deps = {"b"}; tables[1].match_deps = {"a"}; }
    SECTION("action") { tables[0].action_deps = {"b"}; tables[1].action_deps = {"a"}; }
    SECTION("mixed") { tables[0].match_deps = {"b"}; tables[1].action_deps = {"a"}; }
    SECTION("self") { tables[0].match_deps = {"a"}; }
    const auto result = build_graph(tables);
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).message.find("cycle") != std::string::npos);
}

TEST_CASE("duplicate dependency edges coalesce to their strongest rule") {
    const std::vector<Table> tables{
        {"a", MemoryType::sram, 1, {}, {}},
        {"b", MemoryType::sram, 1, {"a", "a"}, {"a"}}};
    const auto result = build_graph(tables);
    REQUIRE(std::holds_alternative<Graph>(result));
    const auto& graph = std::get<Graph>(result);
    REQUIRE(graph.incoming[1].size() == 1);
    CHECK(graph.incoming[1][0].gap == 1);
    CHECK(graph.order.size() == 2);
}

TEST_CASE("graph supports empty programs and rejects duplicate names") {
    CHECK(std::holds_alternative<Graph>(build_graph({})));
    const std::vector<Table> tables{
        {"same", MemoryType::sram, 1, {}, {}}, {"same", MemoryType::sram, 2, {}, {}}};
    CHECK(std::holds_alternative<Error>(build_graph(tables)));
}

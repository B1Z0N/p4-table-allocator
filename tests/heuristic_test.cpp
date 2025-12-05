#include "p4alloc/allocator.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace p4alloc;

static Allocation constrained(const Program& program) {
    const auto result = allocate(program, ConstrainedAllocator{});
    REQUIRE(std::holds_alternative<Allocation>(result));
    return std::get<Allocation>(result);
}

TEST_CASE("constrained heuristic fits a chain that plain greedy blocks") {
    const Program p{{3, 10, 0}, {
        {"background", MemoryType::sram, 6, {}, {}},
        {"classify", MemoryType::sram, 6, {}, {}},
        {"route", MemoryType::sram, 6, {"classify"}, {}},
        {"rewrite", MemoryType::sram, 4, {"route"}, {}}}};
    const auto baseline = allocate(p, GreedyAllocator{});
    REQUIRE(std::holds_alternative<Allocation>(baseline));
    CHECK_FALSE(std::get<Allocation>(baseline).fits);
    const auto improved = constrained(p);
    REQUIRE(improved.fits);
    CHECK(improved.first_stage[1] == 0);
    CHECK(improved.first_stage[2] == 1);
    CHECK(improved.first_stage[3] == 2);
    CHECK(improved.first_stage[0] == 2);
    CHECK(improved.algorithm == "constrained");
}

TEST_CASE("heuristic reduces pipeline depth for a shorter chain") {
    const Program p{{3, 10, 0}, {
        {"background", MemoryType::sram, 6, {}, {}},
        {"classify", MemoryType::sram, 6, {}, {}},
        {"rewrite", MemoryType::sram, 4, {"classify"}, {}}}};
    const auto baseline = allocate(p, GreedyAllocator{});
    REQUIRE(std::holds_alternative<Allocation>(baseline));
    CHECK(std::get<Allocation>(baseline).stages_used() == 3);
    const auto improved = constrained(p);
    REQUIRE(improved.fits);
    CHECK(improved.stages_used() == 2);
}

TEST_CASE("heuristic breaks equal critical paths by relative memory pressure") {
    const Program p{{2, 100, 10}, {
        {"sram", MemoryType::sram, 50, {}, {}},
        {"tcam", MemoryType::tcam, 8, {}, {}}}};
    const auto a = constrained(p);
    REQUIRE(a.fits);
    CHECK(a.stages[0].tables.front().table == 1);
}

TEST_CASE("heuristic only schedules ready tables and ties by input order") {
    const Program p{{4, 10, 10}, {
        {"consumer", MemoryType::sram, 1, {"producer"}, {}},
        {"producer", MemoryType::sram, 1, {}, {}},
        {"tie", MemoryType::tcam, 1, {}, {}}}};
    const auto a = constrained(p);
    REQUIRE(a.fits);
    CHECK(a.first_stage[0].value() > a.last_stage[1].value());
    CHECK(a.first_stage == constrained(p).first_stage);
}

TEST_CASE("heuristic preserves splitting and validates errors") {
    const Program p{{3, 10, 0}, {{"big", MemoryType::sram, 15, {}, {}},
                                  {"tail", MemoryType::sram, 1, {"big"}, {}}}};
    const auto a = constrained(p);
    REQUIRE(a.fits);
    CHECK(a.last_stage[0] == 1);
    CHECK(a.first_stage[1] == 2);
    auto bad = p;
    bad.tables[0].match_deps = {"tail"};
    CHECK(std::holds_alternative<Error>(allocate(bad, ConstrainedAllocator{})));
}

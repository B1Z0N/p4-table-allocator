#include "p4alloc/allocator.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace p4alloc;

static Allocation greedy(const Program& p) {
    const auto result = allocate(p, GreedyAllocator{});
    REQUIRE(std::holds_alternative<Allocation>(result));
    return std::get<Allocation>(result);
}

TEST_CASE("greedy places tables in earliest dependency safe stage") {
    const Program p{{4, 100, 20}, {
        {"acl", MemoryType::tcam, 10, {}, {"route"}},
        {"route", MemoryType::tcam, 15, {"eth"}, {}},
        {"eth", MemoryType::sram, 30, {}, {}}}};
    const auto a = greedy(p);
    REQUIRE(a.fits);
    CHECK(a.first_stage[2] == 0);
    CHECK(a.first_stage[1] == 1);
    CHECK(a.first_stage[0] == 2);
    CHECK(a.stages_used() == 3);
    CHECK(a.stages[0].sram_used == 30);
    CHECK(a.stages[1].tcam_used == 15);
    CHECK_FALSE(a.failure.has_value());
}

TEST_CASE("action dependencies allow sharing but never preceding a producer") {
    const Program p{{3, 10, 10}, {
        {"a", MemoryType::sram, 10, {}, {}},
        {"b", MemoryType::sram, 2, {"a"}, {}},
        {"c", MemoryType::tcam, 4, {}, {"b"}},
        {"d", MemoryType::sram, 3, {}, {"b"}}}};
    const auto a = greedy(p);
    REQUIRE(a.fits);
    CHECK(a.first_stage[1] == 1);
    CHECK(a.first_stage[2] == 1);
    CHECK(a.first_stage[3] == 1);
}

TEST_CASE("SRAM and TCAM budgets are independent") {
    const Program p{{1, 10, 5}, {
        {"a", MemoryType::sram, 10, {}, {}}, {"b", MemoryType::tcam, 5, {}, {}}}};
    const auto a = greedy(p);
    REQUIRE(a.fits);
    CHECK(a.stages[0].sram_used == 10);
    CHECK(a.stages[0].tcam_used == 5);
}

TEST_CASE("does not fit reports a partial placement without exceeding memory") {
    const Program p{{1, 10, 5}, {
        {"a", MemoryType::sram, 8, {}, {}}, {"b", MemoryType::sram, 3, {}, {}}}};
    const auto a = greedy(p);
    CHECK_FALSE(a.fits);
    REQUIRE(a.failure.has_value());
    CHECK(a.failure->find("b") != std::string::npos);
    CHECK(a.stages[0].sram_used == 8);
    CHECK_FALSE(a.first_stage[1].has_value());
}

TEST_CASE("dependency chain can exceed pipeline depth with memory available") {
    const Program p{{1, 100, 100}, {
        {"a", MemoryType::sram, 1, {}, {}}, {"b", MemoryType::sram, 1, {"a"}, {}}}};
    CHECK_FALSE(greedy(p).fits);
}

TEST_CASE("zero sized tables still obey ordering and zero capacity rejects positive sizes") {
    const Program p{{2, 0, 0}, {
        {"a", MemoryType::sram, 0, {}, {}}, {"b", MemoryType::tcam, 0, {"a"}, {}}}};
    const auto a = greedy(p);
    REQUIRE(a.fits);
    CHECK(a.first_stage[0] == 0);
    CHECK(a.first_stage[1] == 1);
    CHECK(a.stages_used() == 2);
    auto positive = p;
    positive.tables[0].size = 1;
    CHECK_FALSE(greedy(positive).fits);
}

TEST_CASE("empty program uses zero stages") {
    const auto a = greedy(Program{{2, 0, 0}, {}});
    CHECK(a.fits);
    CHECK(a.stages_used() == 0);
}

TEST_CASE("allocation is repeatable and does not mutate input") {
    const Program p{{2, 10, 0}, {{"a", MemoryType::sram, 7, {}, {}},
                                  {"b", MemoryType::sram, 4, {}, {}}}};
    CHECK(greedy(p).first_stage == greedy(p).first_stage);
    CHECK(p.tables[0].size == 7);
}

TEST_CASE("maximum memory integer never overflows capacity arithmetic") {
    const auto max = std::numeric_limits<Memory>::max();
    const auto a = greedy(Program{{1, max, 0}, {{"a", MemoryType::sram, max, {}, {}},
                                             {"b", MemoryType::sram, 1, {}, {}}}});
    CHECK_FALSE(a.fits);
    CHECK(a.stages[0].sram_used == max);
}

TEST_CASE("allocation validates programmatic inputs too") {
    Program p{{1, 10, 0}, {{"a", MemoryType::sram, 1, {}, {}}}};
    SECTION("no stages") { p.pipeline.stages = 0; }
    SECTION("negative capacity") { p.pipeline.sram_per_stage = -1; }
    SECTION("negative table size") { p.tables[0].size = -1; }
    SECTION("bad dependency") { p.tables[0].match_deps = {"missing"}; }
    CHECK(std::holds_alternative<Error>(allocate(p, GreedyAllocator{})));
}

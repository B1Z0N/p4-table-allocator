#include "p4alloc/report.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <sstream>

using namespace p4alloc;

TEST_CASE("JSON report preserves fragments budgets and dependency placements") {
    const Program p{{3, 10, 5}, {{"big", MemoryType::sram, 15, {}, {}},
                                  {"tail", MemoryType::tcam, 2, {"big"}, {}}}};
    const auto result = allocate(p, GreedyAllocator{});
    REQUIRE(std::holds_alternative<Allocation>(result));
    const auto report = json_report(p, std::get<Allocation>(result));
    REQUIRE(report.contains("fits"));
    CHECK(report.at("fits") == true);
    CHECK(report.at("algorithm") == "greedy");
    CHECK(report.at("stages_used") == 3);
    CHECK(report.at("pipeline").at("sram_per_stage") == 10);
    CHECK(report.at("stages").at(1).at("tables").at(0).at("size") == 5);
    CHECK(report.at("tables").at(0).at("last_stage") == 1);
    CHECK(report.at("tables").at(1).at("first_stage") == 2);
    CHECK(report.at("error").is_null());
}

TEST_CASE("failed JSON report distinguishes unplaced tables") {
    const Program p{{1, 10, 0}, {{"a", MemoryType::sram, 8, {}, {}},
                                  {"b", MemoryType::sram, 3, {}, {}}}};
    const auto result = allocate(p, GreedyAllocator{});
    const auto report = json_report(p, std::get<Allocation>(result));
    REQUIRE(report.contains("fits"));
    CHECK(report.at("fits") == false);
    CHECK(report.at("tables").at(1).at("first_stage").is_null());
    CHECK(report.at("error").is_string());
}

TEST_CASE("text report prints memory limits stages fragments and fit status") {
    const Program p{{2, 10, 5}, {{"big", MemoryType::sram, 15, {}, {}}}};
    const auto result = allocate(p, GreedyAllocator{});
    std::ostringstream text;
    print_report(text, p, std::get<Allocation>(result));
    CHECK(text.str().find("SRAM") != std::string::npos);
    CHECK(text.str().find("TCAM") != std::string::npos);
    CHECK(text.str().find("big") != std::string::npos);
    CHECK(text.str().find("fits") != std::string::npos);
    CHECK(text.str().find("10/10") != std::string::npos);
}

#include "p4alloc/input.hpp"
#include "p4alloc/report.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <ranges>
#include <span>
#include <string_view>

namespace {

using namespace p4alloc;

constexpr std::string_view usage =
    "Usage: allocator [--json] [--algorithm greedy|constrained | --compare] FILE\n"
    "       allocator --help\n"
    "Default algorithm: constrained. Stage indices start at zero.\n"
    "Exit codes: 0 fits, 1 invalid input/arguments, 2 allocation does not fit.\n";

struct Options {
    std::string path;
    std::string algorithm = "constrained";
    bool json = false;
    bool compare = false;
    bool help = false;
};

Result<Options> parse_options(std::span<char*> arguments) {
    Options options;
    bool algorithm_set = false;
    bool positional_only = false;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string_view arg = arguments[i];
        if (!positional_only && arg == "--") {
            positional_only = true;
        } else if (!positional_only && arg == "--json") {
            options.json = true;
        } else if (!positional_only && arg == "--compare") {
            options.compare = true;
        } else if (!positional_only && arg == "--help") {
            options.help = true;
        } else if (!positional_only && arg == "--algorithm") {
            if (algorithm_set || ++i == arguments.size()) {
                return Error{"--algorithm requires exactly one value: greedy or constrained"};
            }
            options.algorithm = arguments[i];
            algorithm_set = true;
            if (options.algorithm != "greedy" && options.algorithm != "constrained") {
                return Error{"unknown algorithm '" + options.algorithm + "'"};
            }
        } else if (!positional_only && arg.starts_with('-')) {
            return Error{"unknown option '" + std::string(arg) + "'"};
        } else if (options.path.empty()) {
            options.path = arg;
        } else {
            return Error{"expected exactly one input file"};
        }
    }
    if (options.compare && algorithm_set) {
        return Error{"use either --compare or --algorithm"};
    }
    if (options.path.empty() && !options.help) {
        return Error{"missing input file"};
    }
    return options;
}

int print_error(const Error& error, bool json) {
    if (json) {
        std::cout << nlohmann::json{{"error", error.message}}.dump(2) << '\n';
    } else {
        std::cerr << "Error: " << error.message << '\n' << usage;
    }
    return 1;
}

}

int main(int argc, char* argv[]) {
    using namespace p4alloc;
    const auto arguments = std::span(argv, static_cast<std::size_t>(argc)).subspan(1);
    const bool wants_json = std::ranges::any_of(arguments, [](const char* arg) {
        return std::string_view(arg) == "--json";
    });
    const auto options_result = parse_options(arguments);
    if (const auto* error = std::get_if<Error>(&options_result)) {
        return print_error(*error, wants_json);
    }
    const auto& options = std::get<Options>(options_result);
    if (options.help) {
        std::cout << usage;
        return 0;
    }
    const auto program_result = load_input(options.path);
    if (const auto* error = std::get_if<Error>(&program_result)) {
        return print_error(*error, options.json);
    }
    const auto& program = std::get<Program>(program_result);
    if (options.compare) {
        const auto greedy_result = allocate(program, GreedyAllocator{});
        if (const auto* error = std::get_if<Error>(&greedy_result)) {
            return print_error(*error, options.json);
        }
        const auto improved_result = allocate(program, ConstrainedAllocator{});
        if (const auto* error = std::get_if<Error>(&improved_result)) {
            return print_error(*error, options.json);
        }
        const auto& greedy = std::get<Allocation>(greedy_result);
        const auto& improved = std::get<Allocation>(improved_result);
        if (options.json) {
            std::cout << nlohmann::json{{"greedy", json_report(program, greedy)},
                                       {"constrained", json_report(program, improved)}}.dump(2) << '\n';
        } else {
            print_report(std::cout, program, greedy);
            std::cout << '\n';
            print_report(std::cout, program, improved);
        }
        return greedy.fits || improved.fits ? 0 : 2;
    }
    const auto result = options.algorithm == "greedy" ? allocate(program, GreedyAllocator{})
                                                       : allocate(program, ConstrainedAllocator{});
    if (const auto* error = std::get_if<Error>(&result)) {
        return print_error(*error, options.json);
    }
    const auto& allocation = std::get<Allocation>(result);
    if (options.json) {
        std::cout << json_report(program, allocation).dump(2) << '\n';
    } else {
        print_report(std::cout, program, allocation);
    }
    return allocation.fits ? 0 : 2;
}

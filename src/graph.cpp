#include "p4alloc/graph.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <queue>
#include <ranges>
#include <unordered_map>

namespace p4alloc {

Result<Graph> build_graph(std::span<const Table> tables) {
    const auto count = tables.size();
    Graph graph{std::vector<std::vector<Edge>>(count), std::vector<std::vector<Edge>>(count),
                {}, std::vector<std::size_t>(count, 0)};
    std::unordered_map<std::string, std::size_t> names;
    for (std::size_t i = 0; i < count; ++i) {
        if (!names.emplace(tables[i].name, i).second) {
            return Error{"duplicate table name '" + tables[i].name + "'"};
        }
    }
    std::vector<std::size_t> indegrees(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        std::map<std::size_t, std::size_t> dependencies;
        for (const auto gap : {0U, 1U}) {
            const auto& list = gap == 0 ? tables[i].action_deps : tables[i].match_deps;
            for (const auto& name : list) {
                const auto found = names.find(name);
                if (found == names.end()) {
                    return Error{"table '" + tables[i].name + "': unknown dependency '" + name + "'"};
                }
                auto& rule = dependencies[found->second];
                rule = std::max(rule, static_cast<std::size_t>(gap));
            }
        }
        for (const auto& [dependency, gap] : dependencies) {
            graph.incoming[i].push_back({dependency, gap});
            graph.outgoing[dependency].push_back({i, gap});
        }
        indegrees[i] = dependencies.size();
    }
    std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<>> ready;
    for (std::size_t i = 0; i < count; ++i) {
        if (indegrees[i] == 0) {
            ready.push(i);
        }
    }
    while (!ready.empty()) {
        const auto table = ready.top();
        ready.pop();
        graph.order.push_back(table);
        for (const auto& edge : graph.outgoing[table]) {
            if (--indegrees[edge.table] == 0) {
                ready.push(edge.table);
            }
        }
    }
    if (graph.order.size() != count) {
        std::string blocked;
        for (std::size_t i = 0; i < count; ++i) {
            if (indegrees[i] != 0) {
                blocked += (blocked.empty() ? "" : ", ") + tables[i].name;
            }
        }
        return Error{"dependency cycle; unresolved tables: " + blocked};
    }
    for (const auto table : graph.order | std::views::reverse) {
        for (const auto& edge : graph.outgoing[table]) {
            graph.critical_path[table] = std::max(graph.critical_path[table],
                                                 edge.gap + graph.critical_path[edge.table]);
        }
    }
    return graph;
}

}

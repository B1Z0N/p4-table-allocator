#pragma once

#include "p4alloc/model.hpp"
#include <span>

namespace p4alloc {

struct Edge {
    std::size_t table;
    std::size_t gap;
};

struct Graph {
    std::vector<std::vector<Edge>> incoming;
    std::vector<std::vector<Edge>> outgoing;
    std::vector<std::size_t> order;
    std::vector<std::size_t> critical_path;
};

Result<Graph> build_graph(std::span<const Table> tables);

}

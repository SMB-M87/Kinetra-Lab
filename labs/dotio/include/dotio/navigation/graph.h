#ifndef GRAPH_H
#define GRAPH_H

#include <unordered_map>

#include "dotio/navigation/grid.h"
#include "dotio/navigation/priority_queue.h"
#include "dotio/navigation/stack.h"
#include "dotio/navigation/vertex.h"

namespace dotio {
/// Eight-neighbor route search and fear-biased shelter search over the original map.
class Graph {
  public:
    std::unordered_map<dotio::math::GridEntry, Vertex> vertexMap;

    Graph();

    void ClearAll();
    void AddVertex(const dotio::math::GridEntry &cell);
    Vertex &GetVertex(const dotio::math::GridEntry &cell);
    bool AStar(const dotio::math::GridEntry &start, const dotio::math::GridEntry &goal, Vertex &output);
    bool HideStar(const dotio::math::GridEntry &start, const dotio::math::Position &player, const float fear,
                  Vertex &output);
    dotio::Stack GetPath(const dotio::math::GridEntry &start, const Vertex &goal);
};
} // namespace dotio
#endif

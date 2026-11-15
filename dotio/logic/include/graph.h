#ifndef GRAPH_H
#define GRAPH_H

#include <unordered_map>

#include "grid.h"
#include "priority_queue.h"
#include "stack.h"
#include "vertex.h"

namespace logic {
/// Eight-neighbor route search and fear-biased shelter search over the original map.
class Graph {
  public:
    std::unordered_map<gmath::GridEntry, Vertex> vertexMap;

    Graph();

    void ClearAll();
    void AddVertex(const gmath::GridEntry &cell);
    Vertex &GetVertex(const gmath::GridEntry &cell);
    bool AStar(const gmath::GridEntry &start, const gmath::GridEntry &goal, Vertex &output);
    bool HideStar(const gmath::GridEntry &start, const gmath::Position &player, const float fear,
                  Vertex &output);
    logic::Stack GetPath(const gmath::GridEntry &start, const Vertex &goal);
};
} // namespace logic
#endif

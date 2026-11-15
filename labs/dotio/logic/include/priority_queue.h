#pragma once
#include "vertex.h"
#include <queue>
#include <vector>
namespace logic {
/// Unbounded A* frontier; capture priority at insertion so later vertex changes cannot corrupt the heap.
class PriorityQueue {
    struct Entry {
        Vertex *vertex;
        float priority;
        bool operator<(const Entry &other) const { return priority > other.priority; }
    };
    std::priority_queue<Entry> entries_;
    gmath::GridEntry goal_;

  public:
    explicit PriorityQueue(const gmath::GridEntry &goal) : goal_(goal) {}
    void push(Vertex &vertex) { entries_.push({&vertex, vertex.Calculate(goal_)}); }
    Vertex &pop() {
        if (entries_.empty())
            throw std::out_of_range("Empty Dotio frontier");
        auto *vertex = entries_.top().vertex;
        entries_.pop();
        return *vertex;
    }
    bool empty() const { return entries_.empty(); }
    int size() const { return int(entries_.size()); }
    void clear() { entries_ = {}; }
};
} // namespace logic

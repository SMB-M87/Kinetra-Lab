#ifndef VERTEX_H
#define VERTEX_H

#include <list>

#include "dotio/math/gmath.h"

namespace dotio {
class Vertex {
  public:
    dotio::math::GridEntry cell;
    float G, H, F;
    dotio::math::GridEntry prev;
    int steps;

    int straightIndex{};
    int diagonalIndex{};
    dotio::math::GridEntry straight[4]{};
    dotio::math::GridEntry diagonal[4]{};

    Vertex();
    Vertex(const dotio::math::GridEntry &cell);
    Vertex(const Vertex &other);
    Vertex &operator=(const Vertex &other);

    void Reset();
    void AddAdjacent(dotio::Vertex &source, const dotio::math::GridEntry &dest);
    float Calculate(const dotio::math::GridEntry &goal);
    static Vertex NullVertex;
};
} // namespace dotio
#endif

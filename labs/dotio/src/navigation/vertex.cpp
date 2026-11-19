#include "dotio/navigation/vertex.h"
#include "dotio/math/portable.h"

namespace dotio {
Vertex::Vertex() : G(FLT_MAX), H(0), F(FLT_MAX), prev(dotio::math::GridEntry::NullGridEntry), steps(0) {
}

Vertex::Vertex(const dotio::math::GridEntry &cell)
    : cell(cell), G(FLT_MAX), H(0), F(FLT_MAX), prev(dotio::math::GridEntry::NullGridEntry), steps(0) {
}

Vertex::Vertex(const Vertex &other)
    : cell(other.cell), G(other.G), H(other.H), F(other.F), prev(other.prev), steps(other.steps) {
    for (int i = 0; i < other.straightIndex; i++) {
        straight[i] = other.straight[i];
    }

    straightIndex = other.straightIndex;

    for (int i = 0; i < other.diagonalIndex; i++) {
        diagonal[i] = other.diagonal[i];
    }

    diagonalIndex = other.diagonalIndex;
}

Vertex &Vertex::operator=(const Vertex &other) {
    if (this != &other) {
        cell = other.cell;
        prev = other.prev;
        G = other.G;
        H = other.H;
        F = other.F;
        steps = other.steps;

        for (int i = 0; i < other.straightIndex; i++) {
            straight[i] = other.straight[i];
        }
        straightIndex = other.straightIndex;

        for (int i = 0; i < other.diagonalIndex; i++) {
            diagonal[i] = other.diagonal[i];
        }
        diagonalIndex = other.diagonalIndex;
    }
    return *this;
}

void Vertex::Reset() {
    prev = dotio::math::GridEntry::NullGridEntry;
    G = FLT_MAX;
    H = 0;
    F = FLT_MAX;
    steps = 0;
}

void Vertex::AddAdjacent(dotio::Vertex &source, const dotio::math::GridEntry &dest) {
    if (source.cell.Row != dest.Row && source.cell.Column != dest.Column)
        source.diagonal[source.diagonalIndex++] = {dest.Row, dest.Column};
    else
        source.straight[source.straightIndex++] = {dest.Row, dest.Column};
}

float Vertex::Calculate(const dotio::math::GridEntry &goal) {
    const int row = std::abs(cell.Row - goal.Row);
    const int column = std::abs(cell.Column - goal.Column);
    H = float(std::max(row, column)) + (std::numbers::sqrt2_v<float> - 1) * std::min(row, column);
    F = G + H;
    return F;
}

Vertex Vertex::NullVertex = Vertex();
} // namespace dotio

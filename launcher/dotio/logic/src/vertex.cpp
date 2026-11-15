#include "vertex.h"
#include "portable.h"

namespace logic {
Vertex::Vertex() : G(FLT_MAX), H(0), F(FLT_MAX), prev(gmath::GridEntry::NullGridEntry), steps(0) {
}

Vertex::Vertex(const gmath::GridEntry &cell)
    : cell(cell), G(FLT_MAX), H(0), F(FLT_MAX), prev(gmath::GridEntry::NullGridEntry), steps(0) {
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
    prev = gmath::GridEntry::NullGridEntry;
    G = FLT_MAX;
    H = 0;
    F = FLT_MAX;
    steps = 0;
}

void Vertex::AddAdjacent(logic::Vertex &source, const gmath::GridEntry &dest) {
    if (source.cell.Row != dest.Row && source.cell.Column != dest.Column)
        source.diagonal[source.diagonalIndex++] = {dest.Row, dest.Column};
    else
        source.straight[source.straightIndex++] = {dest.Row, dest.Column};
}

float Vertex::Calculate(const gmath::GridEntry &goal) {
    const int row = std::abs(cell.Row - goal.Row);
    const int column = std::abs(cell.Column - goal.Column);
    H = float(std::max(row, column)) + (std::numbers::sqrt2_v<float> - 1) * std::min(row, column);
    F = G + H;
    return F;
}

Vertex Vertex::NullVertex = Vertex();
} // namespace logic

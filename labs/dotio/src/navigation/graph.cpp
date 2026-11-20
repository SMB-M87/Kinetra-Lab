#include "dotio/navigation/graph.h"
#include "dotio/math/portable.h"

namespace dotio {
Graph::Graph() : vertexMap() {
}

void Graph::ClearAll() {
    for (auto &pair : vertexMap) {
        Vertex &vertex = pair.second;
        vertex.Reset();
    }
}

void Graph::AddVertex(const dotio::math::GridEntry &cell) {
    if (vertexMap.find(cell) == vertexMap.end()) {
        vertexMap[cell] = Vertex(cell);
    }
}

Vertex &Graph::GetVertex(const dotio::math::GridEntry &cell) {
    if (vertexMap.find(cell) != vertexMap.end())
        return vertexMap[cell];
    else
        return dotio::Vertex::NullVertex;
}

bool Graph::AStar(const dotio::math::GridEntry &start, const dotio::math::GridEntry &goal, Vertex &output) {
    ClearAll();
    dotio::PriorityQueue q(goal);

    Vertex *vertex = &GetVertex(start);
    vertex->G = 0;
    vertex->Calculate(goal);
    vertex->steps = 0;
    q.push(*vertex);

    while (!q.empty()) {
        vertex = &q.pop();

        if (vertex->cell == goal) {
            output = *vertex;
            return true;
        }

        for (int i = 0; i < vertex->straightIndex; i++) {
            Vertex &straightVertex = GetVertex(vertex->straight[i]);
            float G = vertex->G + 1.0f;
            if (G < straightVertex.G) {
                straightVertex.prev = vertex->cell;
                straightVertex.G = G;
                straightVertex.Calculate(goal);
                straightVertex.steps = vertex->steps + 1;
                q.push(straightVertex);
            }
        }

        for (int i = 0; i < vertex->diagonalIndex; i++) {
            Vertex &diagonalVertex = GetVertex(vertex->diagonal[i]);
            float G = vertex->G + std::numbers::sqrt2_v<float>;
            if (G < diagonalVertex.G) {
                diagonalVertex.prev = vertex->cell;
                diagonalVertex.G = G;
                diagonalVertex.Calculate(goal);
                diagonalVertex.steps = vertex->steps + 1;
                q.push(diagonalVertex);
            }
        }
    }
    return false;
}

bool Graph::HideStar(const dotio::math::GridEntry &start, const dotio::math::Position &player,
                     const float fear, Vertex &output) {
    ClearAll();
    dotio::PriorityQueue q(start);

    dotio::math::GridEntry player_output;
    dotio::math::GridEntry::FromWorld(player, dotio::Grid::Origin, dotio::Grid::CellSize,
                                      dotio::Grid::GridSize, player_output);
    int row = start.Row - player_output.Row;
    int column = start.Column - player_output.Column;
    bool up = row == 0 ? true : row < 0 ? true : false;
    bool down = row == 0 ? true : row > 0 ? true : false;
    bool left = column == 0 ? true : column < 0 ? true : false;
    bool right = column == 0 ? true : column > 0 ? true : false;

    Vertex *vertex = &GetVertex(start);
    vertex->G = 0;
    vertex->steps = 0;
    q.push(*vertex);
    int count = 0;

    while (!q.empty()) {
        vertex = &q.pop();
        dotio::math::Point2 intersection{dotio::math::GridEntry::ToWorld(
            vertex->cell.Row, vertex->cell.Column, dotio::Grid::CellSize, dotio::Grid::Origin)};
        intersection.X += dotio::Grid::CellSize / 2;
        intersection.Y += dotio::Grid::CellSize / 2;

        if (dotio::Grid::IsIntersectingAnyCell(
                {intersection, player}, reinterpret_cast<const int *>(dotio::Grid::Array), intersection)) {
            count++;
            if (count > fear) {
                output = *vertex;
                return true;
            }
        }

        for (int i = 0; i < vertex->straightIndex; i++) {
            Vertex &straightVertex = GetVertex(vertex->straight[i]);

            float G = vertex->G + 1.0f;
            if (G < straightVertex.G &&
                ((((up && straightVertex.cell.Row - player_output.Row <= 0) &&
                   ((left && straightVertex.cell.Column - player_output.Column < 0) ||
                    (right && straightVertex.cell.Column - player_output.Column > 0))) ||
                  ((down && straightVertex.cell.Row - player_output.Row >= 0) &&
                   ((left && straightVertex.cell.Column - player_output.Column < 0) ||
                    (right && straightVertex.cell.Column - player_output.Column > 0))) ||
                  ((left && straightVertex.cell.Column - player_output.Column <= 0) &&
                   ((up && straightVertex.cell.Row - player_output.Row < 0) ||
                    (down && straightVertex.cell.Row - player_output.Row > 0))) ||
                  ((right && straightVertex.cell.Column - player_output.Column >= 0) &&
                   ((up && straightVertex.cell.Row - player_output.Row < 0) ||
                    (down && straightVertex.cell.Row - player_output.Row > 0)))) &&
                 (abs(straightVertex.cell.Row - player_output.Row) > 1 ||
                  abs(straightVertex.cell.Column - player_output.Column) > 1))) {
                straightVertex.prev = vertex->cell;
                straightVertex.G = G;
                straightVertex.steps = vertex->steps + 1;
                q.push(straightVertex);
            }
        }

        for (int i = 0; i < vertex->diagonalIndex; i++) {
            Vertex &diagonalVertex = GetVertex(vertex->diagonal[i]);

            float G = vertex->G + std::numbers::sqrt2_v<float>;
            if (G < diagonalVertex.G &&
                ((((up && diagonalVertex.cell.Row - player_output.Row <= 0) &&
                   ((left && diagonalVertex.cell.Column - player_output.Column < 0) ||
                    (right && diagonalVertex.cell.Column - player_output.Column > 0))) ||
                  ((down && diagonalVertex.cell.Row - player_output.Row >= 0) &&
                   ((left && diagonalVertex.cell.Column - player_output.Column < 0) ||
                    (right && diagonalVertex.cell.Column - player_output.Column > 0))) ||
                  ((left && diagonalVertex.cell.Column - player_output.Column <= 0) &&
                   ((up && diagonalVertex.cell.Row - player_output.Row < 0) ||
                    (down && diagonalVertex.cell.Row - player_output.Row > 0))) ||
                  ((right && diagonalVertex.cell.Column - player_output.Column >= 0) &&
                   ((up && diagonalVertex.cell.Row - player_output.Row < 0) ||
                    (down && diagonalVertex.cell.Row - player_output.Row > 0)))) &&
                 (abs(diagonalVertex.cell.Row - player_output.Row) > 1 ||
                  abs(diagonalVertex.cell.Column - player_output.Column) > 1))) {
                diagonalVertex.prev = vertex->cell;
                diagonalVertex.G = G;
                diagonalVertex.steps = vertex->steps + 1;
                q.push(diagonalVertex);
            }
        }
    }
    return false;
}

dotio::Stack Graph::GetPath(const dotio::math::GridEntry &, const Vertex &goal) {
    Vertex vertex = goal;
    dotio::Stack path;
    int end = goal.steps;
    bool smaller = end <= path.capacity() - 1;
    int boundary = smaller ? end : end - path.capacity();
    int index = 0;

    if (!smaller)
        while (index++ < boundary)
            vertex = GetVertex(vertex.prev);

    while (index++ <= end) {
        path.push(vertex.cell);
        vertex = GetVertex(vertex.prev);
    }

    return path;
}
} // namespace dotio

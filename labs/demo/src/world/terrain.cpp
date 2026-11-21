#include "demo/world/terrain.h"

#include <cmath>
#include <random>
#include <stdexcept>

namespace demo {
bool Terrain::blocked(const kinetra::Body &body) const {
    const auto area = kinetra::bounds(body);
    if (!kinetra::finite(area.min) || !kinetra::finite(area.max) || std::abs(area.min.x) > coordinate_limit ||
        std::abs(area.min.y) > coordinate_limit || std::abs(area.max.x) > coordinate_limit ||
        std::abs(area.max.y) > coordinate_limit)
        return true;
    for (int x = int(std::floor(area.min.x / chunk_size)); x <= int(std::floor(area.max.x / chunk_size)); ++x)
        for (int y = int(std::floor(area.min.y / chunk_size)); y <= int(std::floor(area.max.y / chunk_size));
             ++y)
            for (const auto &obstacle : generate({x, y}))
                if (kinetra::collide(body, obstacle))
                    return true;
    return false;
}

bool Terrain::update(kinetra::Aabb view, int margin) {
    using namespace kinetra;
    if (!finite(view.min) || !finite(view.max) || view.min.x > view.max.x || view.min.y > view.max.y ||
        view.min.x < -coordinate_limit || view.min.y < -coordinate_limit || view.max.x > coordinate_limit ||
        view.max.y > coordinate_limit)
        throw std::invalid_argument("Terrain view must be ordered and within +/-200000 world units");
    const Coordinate first{int(std::floor(view.min.x / chunk_size)) - margin,
                           int(std::floor(view.min.y / chunk_size)) - margin};
    const Coordinate last{int(std::floor(view.max.x / chunk_size)) + margin,
                          int(std::floor(view.max.y / chunk_size)) + margin};
    if (std::int64_t(last.first - first.first + 1) * (last.second - first.second + 1) > maximum_chunks)
        throw std::invalid_argument("Terrain view exceeds 4096 chunks; zoom in or reduce window size");
    bool changed = false;
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        const auto [x, y] = it->first;
        if (x < first.first || x > last.first || y < first.second || y > last.second) {
            it = chunks_.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    for (int x = first.first; x <= last.first; ++x)
        for (int y = first.second; y <= last.second; ++y) {
            const Coordinate coordinate{x, y};
            if (!chunks_.contains(coordinate)) {
                chunks_.emplace(coordinate, generate(coordinate));
                changed = true;
            }
        }
    return changed;
}

std::vector<kinetra::Body> Terrain::generate(Coordinate coordinate) const {
    using namespace kinetra;
    std::seed_seq sequence{seed_, std::uint32_t(coordinate.first), std::uint32_t(coordinate.second)};
    std::mt19937 random(sequence);
    const auto unit = [&]() { return float(random() >> 8) / 16777216.f; };
    const int count = 5 + int(random() % 5);
    std::vector<Body> result;
    for (int i = 0; i < count; ++i) {
        Body body;
        body.motion = Motion::fixed;
        body.shape = random() % 2 ? Shape::box : Shape::circle;
        body.position = {float(coordinate.first) * chunk_size + 4 + unit() * 24,
                         float(coordinate.second) * chunk_size + 4 + unit() * 24};
        body.half_extents = {0.6f + unit() * 1.8f, 0.6f + unit() * 1.8f};
        body.radius = 0.7f + unit() * 1.7f;
        body.angle = unit() * 6.28318530718f;
        body.friction = 0;
        body.restitution = 0.8f;
        const Aabb area = bounds(body);
        if (area.min.x <= 32 && area.max.x >= -32 && area.min.y <= 19 && area.max.y >= -19)
            continue;
        bool overlaps = false;
        for (const auto &other : result)
            if (collide(body, other)) {
                overlaps = true;
                break;
            }
        if (!overlaps)
            result.push_back(body);
    }
    return result;
}
} // namespace demo

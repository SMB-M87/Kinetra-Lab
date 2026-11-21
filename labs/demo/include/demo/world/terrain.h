#pragma once

#include <kinetra/simulation/physics.h>

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace demo {

/// Camera-driven, disposable obstacle chunks. A seed and chunk coordinate fully
/// determine terrain; no visited-area history or SDL state is retained.
class Terrain {
  public:
    static constexpr float chunk_size = 32;
    static constexpr float coordinate_limit = 200000;
    static constexpr int maximum_chunks = 4096;
    using Coordinate = std::pair<int, int>;
    using Chunks = std::map<Coordinate, std::vector<kinetra::Body>>;

    explicit Terrain(std::uint32_t seed) : seed_(seed) {}
    const Chunks &chunks() const { return chunks_; }
    std::uint32_t seed() const { return seed_; }

    /// Query seeded terrain without making a distant Ghost destination resident.
    bool blocked(const kinetra::Body &body) const;

    /// Load requested chunks plus an optional collision margin; evict all others.
    /// Returns whether physics needs new obstacles. Rejects invalid/oversized views
    /// before integer conversion or mutation; supports negative world coordinates.
    bool update(kinetra::Aabb view, int margin = 1);

  private:
    std::uint32_t seed_;
    Chunks chunks_;

    /// Seed each chunk separately so exploration order cannot affect generation.
    /// Keep shapes inside their chunk and preserve an obstacle-free spawn clearing.
    std::vector<kinetra::Body> generate(Coordinate coordinate) const;
};

} // namespace demo

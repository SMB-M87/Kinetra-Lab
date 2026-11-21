#pragma once

#include <kinetra/simulation/physics.h>

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace demo {

/// Camera-driven, disposable obstacle chunks. A seed and chunk coordinate fully
/// determine terrain; no visited-area history or SDL state is retained.
///
/// Each chunk is a square of chunk_size world units holding up to 9 fixed circles and
/// rotated boxes, none overlapping another of the same chunk. The area around the
/// player's start (x within ±32, y within ±19) stays free. A value type: copies share
/// nothing.
class Terrain {
  public:
    /// Side of a chunk in world units.
    static constexpr float chunk_size = 32;
    /// Largest absolute world coordinate a view or a queried body may reach.
    static constexpr float coordinate_limit = 200000;
    /// Most chunks one update() may load, which bounds memory for a far-out zoom.
    static constexpr int maximum_chunks = 4096;
    /// Chunk index: world x and y divided by chunk_size, rounded down.
    using Coordinate = std::pair<int, int>;
    /// The loaded chunks and their fixed obstacles, ordered by coordinate.
    using Chunks = std::map<Coordinate, std::vector<kinetra::Body>>;

    /// Terrain generated from `seed`; no chunk is loaded until update().
    explicit Terrain(std::uint32_t seed) : seed_(seed) {}
    /// The chunks the last update() loaded.
    const Chunks &chunks() const { return chunks_; }
    /// The seed every chunk is generated from.
    std::uint32_t seed() const { return seed_; }

    /// Whether `body` overlaps generated terrain, loaded or not, so a distant ghost
    /// destination can be checked without becoming resident. A body outside
    /// coordinate_limit or with non-finite bounds counts as blocked.
    bool blocked(const kinetra::Body &body) const;

    /// Load the chunks that `view` touches plus `margin` chunks around them; evict all
    /// others. Returns whether a chunk was loaded or evicted, so physics needs new
    /// obstacles. Throws std::invalid_argument, before changing anything, for a
    /// non-finite or unordered view, one beyond coordinate_limit, or one that needs more
    /// than maximum_chunks; negative coordinates are fine.
    bool update(kinetra::Aabb view, int margin = 1);

  private:
    std::uint32_t seed_;
    Chunks chunks_;

    /// Seed each chunk separately so exploration order cannot affect generation.
    /// Keep shapes inside their chunk and preserve an obstacle-free spawn clearing.
    std::vector<kinetra::Body> generate(Coordinate coordinate) const;
};

} // namespace demo

# Performance

Where Lab's applications spend their time, measured on 2026-11-23, and how to measure a change against it. Each optimization is its own pull request, with this page's workload before and after and the golden lines unchanged; it updates the numbers here.

## Table of contents

- [Summary](#summary)
- [How to measure](#how-to-measure)
- [i4sim](#i4sim)
- [Demo](#demo)
- [Dotio](#dotio)
- [Hotspots](#hotspots)
- [Open points](#open-points)

[Documentation index](README.md) · [Project overview](../README.md) · [Roadmap](roadmap.md)

## Summary

The baseline is Lab `bae378f` with Kinetra `bc17b4f`, built in the `release` preset (GCC 13.3, `-O3`) and run headless on a cloud machine with four cores of an Intel Xeon at 2.3 GHz. Absolute times depend on the machine, so compare only runs on one machine. The i4sim times the roadmap quoted before came from a faster machine (9–10.5 s against 13–14.5 s on the large layout); the ratio between the two policies is about the same.

| Application | Workload | Time | Per tick | Where the time goes |
| --- | --- | --- | --- | --- |
| i4sim | Motion benchmark: three layouts, two seeds, 50,000 ticks each | 28.0–28.6 s; 40.1–40.3 s with predictive passing | 0.25 ms on `8640x4800` (106 movers); 0.36 ms with passing | Contact resolution over every pair of movers (46%); with passing, the spatial queries that form plans (23%) |
| Demo | Crowd soak: 60 simulated seconds, 7,200 ticks | 20.8–23.2 s at 240 agents; 9.4–10.0 s at 48 | 3.1 ms at 240 agents; 1.3–1.4 ms at 48, the default | `World::step` rebuilds its spatial grid 14 times per tick, fixed terrain included: 35% at 240 agents, 64% at 48 |
| Dotio | Behavior benchmark defaults: 12 seeds, three difficulties, 180 simulated seconds | 20.7–21.5 s | 0.055 ms, scripted player included | Prop contacts (41%); a shelter search over the whole window that finds nothing costs 7.5 ms |

What this means for a running application:

- **Demo** runs short of time. At the default 48 agents the simulation needs 1.3 ms per tick, so at time scale 4 (480 ticks per second) it takes 0.64 s of every second before anything is drawn, at any frame rate; 240 agents would need 1.5 s per second. The fixed clock's cap of 16 ticks per frame ([Demo open points](../labs/demo/README.md#open-points)) is where this shows, not its cause.
- **i4sim** at its default 200 ticks per second spends 5% of a core on the large layout, 7% with passing. Its ceiling there is about 4,000 ticks per second, 2,700 with passing, so the highest UPS settings fall behind.
- **Dotio** spends 0.3% of a 60 Hz frame on an average tick. Its risk is a spike: a shelter search over the whole window that finds nothing costs 7.5 ms, about half a frame, and a cornered recovering enemy repeats its search at every decision, 20 times a second.

## How to measure

Build the workloads in the `release` preset:

```sh
cmake --preset release
cmake --build --preset release --target kinetra_i4sim_motion_benchmark kinetra_demo_crowd_tests \
    kinetra_demo_crowd_characterization_tests kinetra_dotio_behavior_benchmark
```

| Application | Command | Time it reports |
| --- | --- | --- |
| i4sim | `./build-release/kinetra_i4sim_motion_benchmark`, and again with `--predictive-passing` | `elapsed_ms` per layout and seed ([motion benchmark](../labs/i4sim/tests/README.md#motion-benchmark)) |
| Demo | `./build-release/kinetra_demo_crowd_tests --duration=true` | doctest's time per case; a case runs 7,200 ticks |
| Demo | `./build-release/kinetra_demo_crowd_characterization_tests --duration=true` | the same for the golden scenarios, 1.6–2.2 s each; 90% of it is the simulation, with the same profile as the soak cases |
| Dotio | `time ./build-release/kinetra_dotio_behavior_benchmark` | none of its own; the scripted player's planning is under 1% of the total ([behavior benchmark](../labs/dotio/README.md#behavior-benchmark)) |

For a profile, build a second tree with frame pointers and symbols. It gave the same benchmark results as the release build, within 3% of its time:

```sh
cmake --preset release -B build-profile -DCMAKE_CXX_FLAGS="-g -fno-omit-frame-pointer"
cmake --build build-profile --target kinetra_i4sim_motion_benchmark
perf record -F 499 --call-graph fp -o i4sim.perf -- ./build-profile/kinetra_i4sim_motion_benchmark > /dev/null
perf report -i i4sim.perf --no-children --sort symbol   # time in each function itself
perf report -i i4sim.perf --children --sort symbol      # including what it calls
```

- `perf` comes with the kernel tools of the distribution. In a cloud session on Ubuntu, `linux-tools-generic` installs it as `/usr/lib/linux-tools-<version>/perf`.
- Frame pointers lose the caller of some small functions. `--call-graph dwarf` on a shorter run (`--ticks 20000`) finds it.
- `valgrind --tool=callgrind` counts calls exactly; it runs about 50 times slower, so give it a short run.
- A comparison builds both versions on one machine, runs each three times and reports the median. The workload's results must not change: i4sim's CSV columns other than `elapsed_ms`, Dotio's fingerprints, and Demo's golden lines. Over three runs here, i4sim's total varied by 2% and Demo's cases by 5%.

## i4sim

Run time per layout, both seeds, three runs:

| Layout | Movers | Default | Predictive passing |
| --- | --- | --- | --- |
| `MAS-demonstrator` | 11 | 0.36–0.38 s | 0.41–0.43 s |
| `3840x2400` | 25 | 1.1–1.3 s | 1.3–1.4 s |
| `8640x4800` | 106 | 12.2–13.1 s | 17.8–18.9 s |

The large layout is 88% of the run, so the profile is mostly its profile. Share of the run time, from about 14,000 samples (default) and 20,000 (with passing):

| Where | Default | With passing | Why |
| --- | --- | --- | --- |
| `MotionStep::resolve_contacts` ([motion_step.cpp](../labs/i4sim/src/simulation/motion_step.cpp)) | 46% | 32% | It tests every pair of movers each tick, 5,565 pairs on `8640x4800`, and computes `kinetra::bounds` of both bodies again for each pair. `bounds` alone is 30% of the run (default): two `sincosf` and two projections per box |
| `PredictivePassing::form_plans` ([predictive_passing.cpp](../labs/i4sim/src/simulation/predictive_passing.cpp)) | – | 26% | One `SpatialGrid::query` per eligible mover per tick, over a square that grows with the passing horizon: 23% in the query and its sort. This is nearly all that passing adds, 10.8 s of the 12.1 s difference |
| `MotionStep::choose_acceleration` | 27% | 18% | Mostly `kinetra::priority_steer` (22% and 15%): for every neighbor in reach it tests (horizon + 1)² = 1,089 pairs of predicted positions |
| `MotionStep::surroundings` | 10% | 10% | `SpatialGrid::query` per moving mover (7%) |
| `SpatialGrid::rebuild` | 5% | 4% | Mostly the index of mover bounds, built anew every tick |
| `Production::update` | 4% | 3% | Product scheduling |

`kinetra::validate`, which each steering and collision call runs on every body it is given, takes 4%; allocation takes 3%.

## Demo

| Case | Agents | Time, three runs | Per tick |
| --- | --- | --- | --- |
| Dense crowd, seed 123, steering wander | 240 | 22.2–22.6 s | 3.1 ms |
| Dense crowd, seed 456, steering wander | 240 | 22.6–23.2 s | 3.2 ms |
| Dense crowd, seed 123, route wander | 240 | 20.8–21.2 s | 2.9 ms |
| Standing player, steering wander | 48 | 9.6–10.0 s | 1.3–1.4 ms |
| Standing player, route wander | 48 | 9.4–9.5 s | 1.3 ms |

The world holds 489 bodies at 48 agents and 681 at 240; 362 of them are fixed terrain. Share of the run time, seed 123 with steering wander:

| Where | 240 agents | 48 agents | Why |
| --- | --- | --- | --- |
| `kinetra::World::step` ([physics.cpp](../external/Kinetra/src/simulation/physics.cpp)) | 55% | 88% | Two substeps, each with a sweep pass and six correction passes |
| of which `SpatialGrid::rebuild` | 35% | 64% | Every pass rebuilds the grid from all bodies, at least 14 times per tick and 14.0 on average at 240 agents (callgrind); each rebuild clears a hash map of vectors and fills it again, the fixed terrain included |
| of which `SpatialGrid::query` | 9% | 9% | About 3,900 queries per tick at 240 agents (callgrind): bodies in the sweep passes, moving bodies in the correction passes |
| `Scene::steer_agents` ([scene.cpp](../labs/demo/src/world/scene.cpp)) | 41% | 9% | |
| of which `SpatialGrid::query` | 22% | 5% | Two queries per agent per tick, for neighbors and terrain, each hashing every cell it covers, then allocating and sorting its result |
| of which `kinetra::avoid_collisions` | 11% | 1% | Reciprocal avoidance |

Allocation and freeing, mostly from the grid, take 21% of the run at 240 agents and 34% at 48.

## Dotio

The behavior benchmark ran 388,800 ticks in 20.7–21.5 s over two runs: 55 µs per tick with the scripted player. Its share of the run time:

| Where | Share | Why |
| --- | --- | --- |
| `World::update_objects` ([world.cpp](../labs/dotio/src/world.cpp)) | 41% | Six contact passes; each tests every prop (a crate per loaded room) against the player, the whole enemy pool and every other prop, then against the walls |
| `World::update_enemies` | 20% | Enemy movement, separation and walls |
| `World::stream_enemies` | 5% | Residency and spawning |

The benchmark's enemies recover for at most 0.1% of the time, so the shelter search does not show in the profile. Its worst case was measured on its own: Kinetra's `WeightedGrid::search` on Dotio's 155 × 155 window, with every fourth cell of every fourth row blocked and a predicate that accepts nothing, offers 22,504 cells and takes 7.5 ms. Dotio's own search ([route.cpp](../labs/dotio/src/navigation/route.cpp)) enters only cells on the enemy's side of the player, a quarter to a half of the window, but casts a ray for every cell it is offered. `RecoverState::update` ([state.cpp](../labs/dotio/src/ai/state.cpp)) searches again at every decision, every third update, while the player is within 2.5 cells, or while the enemy sees the player and has no route.

## Hotspots

Ordered by what an optimization would save. The direction is a proposal; its pull request decides.

| # | Hotspot | Repository | Share | Direction |
| --- | --- | --- | --- | --- |
| 1 | `World::step` rebuilds its grid 14 times per tick from every body, fixed ones included | Kinetra | Demo: 64% at 48 agents, 35% at 240 | Index fixed bodies once, when they change, and reuse the bucket storage instead of freeing it. The candidate pairs must come in the same order: the earliest sweep event breaks ties by pair order, and the correction passes resolve in order |
| 2 | i4sim's contact resolution tests every pair and recomputes both bounds per pair | Lab | i4sim: 46%, 32% with passing | Bounds once per mover per tick and the tick's `SpatialGrid` index as broad phase, visiting pairs in the same order |
| 3 | `SpatialGrid::query` hashes every cell, allocates and sorts on every call | Kinetra | i4sim with passing: 23% in plan forming, 7% elsewhere; Demo: 22% at 240 agents | A dense cell array for a bounded area, or a query into the caller's buffer; the result stays sorted and unique |
| 4 | `priority_steer` tests (horizon + 1)² pairs of predicted positions per neighbor | Kinetra | i4sim: 22%, 15% with passing | Compute, for each step of the mover, the first step of the neighbor that overlaps, so the same first pair is found |
| 5 | `bounds` of a box calls `sincosf` twice | Kinetra | i4sim: 7% in `sincosf` | One rotation per call; most of it goes with 2 |
| 6 | Dotio's shelter search that finds nothing, repeated at every decision | Lab | none on average; up to 7.5 ms per search | Bound the search's cost, or search again only when the player or the enemy changes cell; the behavior benchmark gets a time column and a cornered-enemy case |

Not worth an optimization now: Dotio's prop contacts (41% of 0.3% of a frame), i4sim's production scheduling (4%) and `validate` (4%).

## Open points

- The runs are headless: drawing and the runtime are not in this baseline, and neither is the desktop target (Artix). A frame-time check of Demo at time scale 4 belongs in a playtest, with the frame-time graph of the debug menu.
- The Dotio benchmark reports no time, and the shelter search's worst case was measured with a program that is not in the repository; hotspot 6 adds both to the benchmark.

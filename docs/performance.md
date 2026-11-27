# Performance

Where Lab's applications spend their time, measured on 2026-11-23, and how to measure a change against it. Each optimization is its own pull request, with this page's workload before and after and the golden lines unchanged; it updates the numbers here.

## Table of contents

- [Summary](#summary)
- [How to measure](#how-to-measure)
- [i4sim](#i4sim)
- [Demo](#demo)
- [Dotio](#dotio)
- [Hotspots](#hotspots)
- [Measured changes](#measured-changes)
- [Open points](#open-points)

[Documentation index](README.md) · [Project overview](../README.md) · [Roadmap](roadmap.md)

## Summary

The baseline is Lab `bae378f` with Kinetra `bc17b4f`, built in the `release` preset (GCC 13.3, `-O3`) and run headless on a cloud machine with four cores of an Intel Xeon at 2.3 GHz. Absolute times depend on the machine, so compare only runs on one machine. The i4sim times the roadmap quoted before came from a faster machine (9–10.5 s against 13–14.5 s on the large layout); the ratio between the two policies is about the same.

The times in this page's tables are the baseline's. The shares, in *Where the time goes* and in each application's profile, are profiled again after each optimization; the optimization's own before and after are under [Measured changes](#measured-changes).

| Application | Workload | Time | Per tick | Where the time goes |
| --- | --- | --- | --- | --- |
| i4sim | Motion benchmark: three layouts, two seeds, 50,000 ticks each | 28.0–28.6 s; 40.1–40.3 s with predictive passing | 0.25 ms on `8640x4800` (106 movers); 0.36 ms with passing | `priority_steer` (26%, 16% with passing); with passing, the spatial queries that form plans (25%). Contact resolution over every pair of movers was 46% until hotspot 2, the queries 32% with passing until hotspot 3, `priority_steer` 48% until hotspot 4 |
| Demo | Crowd soak: 60 simulated seconds, 7,200 ticks | 20.8–23.2 s at 240 agents; 9.4–10.0 s at 48 | 3.1 ms at 240 agents; 1.3–1.4 ms at 48, the default | Steering (69% at 240 agents) and `World::step` (54% at 48); `SpatialGrid::query` 40% and 32% of the run across both. `World::step` rebuilding its grid 14 times per tick from every body was 35% and 64% until hotspot 1 |
| Dotio | Behavior benchmark defaults: 12 seeds, three difficulties, 180 simulated seconds | 20.7–21.5 s | 0.055 ms, scripted player included | Prop contacts (41%); a shelter search over the whole window that finds nothing costs 7.5 ms |

What this means for a running application:

- **Demo** ran short of time. At the default 48 agents the simulation needed 1.3 ms per tick, so at time scale 4 (480 ticks per second) it took 0.64 s of every second before anything was drawn, at any frame rate; 240 agents would have needed 1.5 s per second. The fixed clock's cap of 16 ticks per frame ([Demo open points](../labs/demo/README.md#open-points)) is where this showed, not its cause. Hotspot 1 cut a tick to 0.22 of its time at 48 agents and 0.65 at 240, measured on another machine; scaled, that is about 0.3 ms and 2.0 ms per tick, 0.14 s and 0.97 s of every second at time scale 4, so 240 agents were still at the limit. Hotspot 3 cut a tick again to 0.56 of its time at 48 agents and 0.70 at 240: about 0.17 ms and 1.4 ms per tick, 0.08 s and 0.68 s of every second at time scale 4.
- **i4sim** at its default 200 ticks per second spent 5% of a core on the large layout, 7% with passing, which put its ceiling there at about 4,000 ticks per second, 2,700 with passing. Hotspot 2 cut a tick on that layout to 0.51 of its time, 0.64 with passing, measured on another machine; scaled, that is about 2.6% and 4.5% of a core, and a ceiling of about 7,800 and 4,200 ticks per second, so the highest UPS settings still fell behind. Hotspot 3 cut a tick on that layout again to 0.80 of its time, 0.67 with passing: in all about 2.0% and 3.0% of a core, a ceiling of about 9,800 and 6,300 ticks per second. Hotspot 4 cut it again to 0.72, 0.83 with passing: in all about 1.5% and 2.5% of a core, a ceiling of about 13,600 and 7,600 ticks per second.
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
| i4sim | `./build-release/kinetra_i4sim_motion_benchmark --ticks 50000`, and again with `--predictive-passing`; the baseline and the measured changes ran 50,000 ticks, the default until 2026-11-27, and the default is now 1,000,000 | `elapsed_ms` and `ticks_per_second` per layout and seed ([motion benchmark](../labs/i4sim/tests/README.md#motion-benchmark)) |
| Demo | `./build-release/kinetra_demo_crowd_tests --duration=true` | doctest's time per case; a case runs 7,200 ticks |
| Demo | `./build-release/kinetra_demo_crowd_characterization_tests --duration=true` | the same for the golden scenarios, 1.6–2.2 s each; 90% of it is the simulation, with the same profile as the soak cases |
| Dotio | `time ./build-release/kinetra_dotio_behavior_benchmark` | none of its own; the scripted player's planning is under 1% of the total ([behavior benchmark](../labs/dotio/README.md#behavior-benchmark)) |

For a profile, build a second tree with frame pointers and symbols. It gave the same benchmark results as the release build, within 3% of its time:

```sh
cmake --preset release -B build-profile -DCMAKE_CXX_FLAGS="-g -fno-omit-frame-pointer"
cmake --build build-profile --target kinetra_i4sim_motion_benchmark
perf record -F 499 --call-graph fp -o i4sim.perf -- ./build-profile/kinetra_i4sim_motion_benchmark --ticks 50000 > /dev/null
perf report -i i4sim.perf --no-children --sort symbol   # time in each function itself
perf report -i i4sim.perf --children --sort symbol      # including what it calls
```

- `perf` comes with the kernel tools of the distribution. In a cloud session on Ubuntu, `linux-tools-generic` installs it as `/usr/lib/linux-tools-<version>/perf`.
- Frame pointers lose the caller of some small functions. `--call-graph dwarf` on a shorter run (`--ticks 20000`) finds it.
- `valgrind --tool=callgrind` counts calls exactly; it runs about 50 times slower, so give it a short run.
- A comparison builds both versions on one machine, runs each three times and reports the median. The workload's results must not change: i4sim's CSV columns other than `elapsed_ms` and `ticks_per_second`, Dotio's fingerprints, and Demo's golden lines. Over three runs here, i4sim's total varied by 2% and Demo's cases by 5%.

## i4sim

Run time per layout in the baseline, both seeds, three runs:

| Layout | Movers | Default | Predictive passing |
| --- | --- | --- | --- |
| `test-environment` | 11 | 0.36–0.38 s | 0.41–0.43 s |
| `3840x2400` | 25 | 1.1–1.3 s | 1.3–1.4 s |
| `8640x4800` | 106 | 12.2–13.1 s | 17.8–18.9 s |

The large layout is 86% of the run after hotspot 2 (88% in the baseline), so the profile is mostly its profile. Share of the run time after hotspot 4, profiled on the machine of its [measured change](#measured-changes) from about 5,700 samples (default) and 9,400 (with passing):

| Where | Default | With passing | Why |
| --- | --- | --- | --- |
| `MotionStep::choose_acceleration` | 39% | 25% | `kinetra::priority_steer` (26% and 16%), which tests only the steps of a neighbor that can overlap each step of the mover; it tested all (horizon + 1)² = 1,089 pairs, 48% and 31%, until hotspot 4. `kinetra::steer` 10% and 7% |
| `PredictivePassing::form_plans` (since then `kinetra::PassingCoordinator::step`, which i4sim's [predictive_passing.cpp](../labs/i4sim/src/simulation/predictive_passing.cpp) calls and which forms the plans) | – | 30% | One `SpatialGrid::query` per eligible mover per tick, over a square that grows with the passing horizon: 25% in the query. Before hotspot 3 the query and its sort were 32% of a longer run |
| `MotionStep::surroundings` | 18% | 15% | `SpatialGrid::query` per moving mover (11% and 10%) |
| `MotionStep::resolve_contacts` ([motion_step.cpp](../labs/i4sim/src/simulation/motion_step.cpp)) | 15% | 11% | It tests every pair of movers each tick, 5,565 pairs on `8640x4800`, on bounds computed once per mover; before hotspot 2 it computed both bounds again for each pair, 46% and 32% of the run. About a third of what is left is the distance from every mover to every border |
| `Production::update` | 13% | 8% | Product scheduling |
| `SpatialGrid::rebuild` | 4% | 2% | Mostly the index of mover bounds, built anew every tick; 9% and 6% before hotspot 3 |

`kinetra::validate`, which each steering and collision call runs on every body it is given, takes 10% (7% with passing); allocation and freeing 6% (7%); `kinetra::bounds` 3% (2%), with `sincosf` 2% (1%).

## Demo

| Case | Agents | Time, three runs | Per tick |
| --- | --- | --- | --- |
| Dense crowd, seed 123, steering wander | 240 | 22.2–22.6 s | 3.1 ms |
| Dense crowd, seed 456, steering wander | 240 | 22.6–23.2 s | 3.2 ms |
| Dense crowd, seed 123, route wander | 240 | 20.8–21.2 s | 2.9 ms |
| Standing player, steering wander | 48 | 9.6–10.0 s | 1.3–1.4 ms |
| Standing player, route wander | 48 | 9.4–9.5 s | 1.3 ms |

The world holds 489 bodies at 48 agents and 681 at 240; 362 of them are fixed terrain. Share of the run time after hotspot 3, profiled on the machine of its [measured change](#measured-changes) from about 6,700 samples (240 agents, seed 123, steering wander) and 1,700 (48 agents, standing player, steering wander):

| Where | 240 agents | 48 agents | Why |
| --- | --- | --- | --- |
| `kinetra::World::step` ([physics.cpp](../external/Kinetra/src/simulation/physics.cpp)) | 26% | 54% | Two substeps, each with a sweep pass and six correction passes. Before hotspot 1 it was 55% and 88% |
| of which `SpatialGrid::rebuild` | 4% | 8% | Every pass rebuilds the index of the moving bodies, at least 14 times per tick; the fixed terrain has an index of its own, rebuilt only when a fixed body changes. Before hotspot 1 every rebuild took all bodies, 35% and 64% |
| of which `SpatialGrid::query` | 11% | 20% | About 3,900 queries per tick at 240 agents (callgrind, baseline): bodies in the sweep passes, moving bodies in the correction passes. A moving body queries both indexes; a fixed body, in the sweep passes, only the moving one |
| `Scene::steer_agents` ([scene.cpp](../labs/demo/src/world/scene.cpp)) | 69% | 31% | |
| of which `SpatialGrid::query` | 29% | 12% | Two queries per agent per tick, for neighbors and terrain. The neighbor query covers the whole avoidance horizon, so it visits many cells and boxes; the query itself no longer hashes, deduplicates or, for one cell, sorts |
| of which `kinetra::avoid_collisions` | 23% | 7% | Reciprocal avoidance |

Allocation and freeing take 6% of the run at 240 agents and 8% at 48; before hotspot 1, 21% and 34%, mostly from the grid.

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
| 1 | `World::step` rebuilds its grid 14 times per tick from every body, fixed ones included | Kinetra | Demo: 64% at 48 agents, 35% at 240 | Done on 2026-11-24: fixed bodies in an index of their own, rebuilt only when one changes, and the bucket storage reused; the candidate pairs keep their order. Demo's soak 78% shorter at 48 agents, 35% at 240 ([measured changes](#measured-changes)) |
| 2 | i4sim's contact resolution tests every pair and recomputes both bounds per pair | Lab | i4sim: 46%, 32% with passing | Done on 2026-11-24: bounds once per mover, the run 47% shorter, 35% with passing ([measured changes](#measured-changes)). A `SpatialGrid` broad phase was slower at 106 movers, so every pair is still tested, on the ready boxes |
| 3 | `SpatialGrid::query` hashes every cell, allocates and sorts on every call | Kinetra | i4sim after hotspot 2: 14%; with passing 32% in plan forming and 10% elsewhere; Demo after hotspot 1: 47% at 240 agents, 43% at 48 | Done on 2026-11-24: a dense cell array for a compact area, each box reported from one cell, and a query into the caller's buffer. i4sim 20% shorter, 33% with passing; Demo's soak 30% shorter at 240 agents, 44% at 48 ([measured changes](#measured-changes)) |
| 4 | `priority_steer` tests (horizon + 1)² pairs of predicted positions per neighbor | Kinetra | i4sim after hotspot 3: 48%, 31% with passing | Done on 2026-11-24: for each step of the mover, only the interval of the neighbor's steps that can overlap, widened by a bound on the rounding error, is tested, so the same first pair is found. i4sim 28% shorter, 18% with passing ([measured changes](#measured-changes)) |
| 5 | `bounds` of a box calls `sincosf` twice | Kinetra | i4sim: 7% in `sincosf`; after hotspot 4 `bounds` 3% (2% with passing), `sincosf` 2% (1%) | Not done, decided on 2026-11-24: after hotspots 1 to 4, one rotation per call would save about half of `sincosf`, about 1% of i4sim, too little for a pull request of its own ([design](../external/Kinetra/docs/design/2026-11-24-engine-hotspots.md#step-4-box-bounds-not-done)) |
| 6 | Dotio's shelter search that finds nothing, repeated at every decision | Lab | none on average; up to 7.5 ms per search | Bound the search's cost, or search again only when the player or the enemy changes cell; the behavior benchmark gets a time column and a cornered-enemy case |

Not worth an optimization now: Dotio's prop contacts (41% of 0.3% of a frame). i4sim's production scheduling and `validate` were 4% each in the baseline and are 13% and 10% after hotspot 4, now the largest shares after steering and the spatial queries; they are the next candidates, under *Open points*.

## Measured changes

Each optimization measures its workload before and after on one machine, in one session, three runs each, and gives the median. Absolute times differ per machine, so compare a row's before and after, not a row with the baseline tables above.

| Date | Change | Workload | Before | After | Change in time |
| --- | --- | --- | --- | --- | --- |
| 2026-11-24 | Hotspot 2: i4sim's contact resolution computes bounds once per mover ([design](design/2026-11-24-i4sim-contact-pairs.md)) | i4sim motion benchmark, default; with `--predictive-passing` | 40.6 s; 59.0 s | 21.6 s; 38.2 s | −47%; −35%. On `8640x4800` 0.36 to 0.19 ms per tick, with passing 0.54 to 0.34 ms. Cloud machine: four cores of an Intel Xeon at 2.1 GHz, GCC 13.3, about 1.45 times slower than the baseline's |
| 2026-11-24 | Hotspot 1: `World::step` indexes fixed bodies once, when they change ([design](../external/Kinetra/docs/design/2026-11-24-engine-hotspots.md)) | Demo crowd soak at 48 agents, steering and route wander; at 240 agents, seeds 123 and 456 and route wander | 12.1 s, 12.0 s; 28.0 s, 28.9 s, 26.0 s | 2.7 s, 2.5 s; 18.1 s, 18.5 s, 16.3 s | −78%, −79%; −35%, −36%, −37%. The golden scenarios 1.0–1.5 s instead of 2.0–2.7 s; i4sim and Dotio within run-to-run noise (−1% to −3%). Cloud machine: four cores of an Intel Xeon at 2.1 GHz, GCC 13.3, about 1.25 times slower than the baseline's on Demo |
| 2026-11-24 | Hotspot 3: `SpatialGrid` stores a compact area densely; a query there hashes nothing, needs no deduplication and sorts only across cells, and can write into the caller's buffer (Lab's callers keep the returning one) ([design](../external/Kinetra/docs/design/2026-11-24-engine-hotspots.md)) | i4sim motion benchmark, default; with `--predictive-passing`. Demo crowd soak at 48 agents, steering and route wander; at 240 agents, seeds 123 and 456 and route wander | 19.8 s; 34.3 s. 2.9 s, 2.6 s; 18.8 s, 18.9 s, 16.0 s | 15.8 s; 23.1 s. 1.6 s, 1.4 s; 13.3 s, 13.4 s, 11.7 s | −20%; −33%. −44%, −45%; −30%, −29%, −27%. On `8640x4800` 0.34 to 0.27 ms per tick, with passing 0.62 to 0.41 ms; Dotio unchanged. Measured on top of hotspots 1 and 2, the same machine as hotspot 1 |
| 2026-11-24 | Hotspot 4: `priority_steer` tests only the steps that can overlap ([design](../external/Kinetra/docs/design/2026-11-24-engine-hotspots.md)) | i4sim motion benchmark, default; with `--predictive-passing` | 15.8 s; 22.5 s | 11.4 s; 18.4 s | −28%; −18%. On `8640x4800` 0.27 to 0.19 ms per tick, with passing 0.40 to 0.33 ms; Demo and Dotio unchanged. Measured on top of hotspots 1 to 3, the same machine |

## Open points

- The runs are headless: drawing and the runtime are not in this baseline, and neither is the desktop target (Artix). A frame-time check of Demo at time scale 4 belongs in a playtest, with the frame-time graph of the debug menu.
- About a third of i4sim's contact resolution is the distance from every mover to every border segment: 3% of the run after hotspot 2, about 5% after hotspot 4, more than `bounds` now. Skipping far segments by their boxes needs a margin that cannot change a result near the threshold.
- After hotspot 4, i4sim's production scheduling (13%) and `kinetra::validate` (10%, run on every body each steering and collision call is given) are the largest shares after steering and the spatial queries. Neither has a roadmap item yet; a new baseline would rank them with the border distances.
- The Dotio benchmark reports no time, and the shelter search's worst case was measured with a program that is not in the repository; hotspot 6 adds both to the benchmark.

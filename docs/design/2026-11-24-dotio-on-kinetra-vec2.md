# Dotio on `kinetra::Vec2`

Status: implemented · Branch: `ccr-5f0ec773-qklel2`, Kinetra `feat/vec2-equality` · Related: roadmap item *Dotio on `kinetra::Vec2`*, [Dotio open points](../../labs/dotio/README.md#open-points), [Dotio restructuring, open points](2026-11-21-dotio-restructuring.md#open-points)

Dotio carries its own vector type, `dotio::math::Vec2`, with seven aliases and some forty operations, while every other Lab application and Kinetra itself use `kinetra::Vec2`. This change puts Dotio on Kinetra's vector, segment and color types and removes `dotio/math/vector.h`, without changing a single decision of the game: the golden fingerprint stays the same. Kinetra gets the one operation Dotio needs and its vector lacks, exact equality; that part is a Kinetra pull request of its own, which this document covers and which merges first.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

- `dotio::math::Vec2` (`labs/dotio/include/dotio/math/vector.h:9`) is a second vector type next to `kinetra::Vec2` (`external/Kinetra/include/kinetra/simulation/math.h:13`), with seven names for the one type (`Vec2`, `Vector2`, `Size2`, `Point2`, `Coord`, `Coordinates`, `Pos`, `Position`; 87 uses of the aliases in Dotio and the capture tool). Its own `Segment` and `RGBA` duplicate `kinetra::Segment` and `kinetra::Color`.
- Since phase 6 its arithmetic already delegates to Kinetra (`labs/dotio/src/math/vector.cpp`), so every boundary converts between the two types for nothing: `physics_vector` and `legacy_vector` (`labs/dotio/include/dotio/entities/physics_adapter.h:7`) copy the same two floats, as do `canonical` and `legacy` in `vector.cpp` and `planner_point` in `navigation/route.cpp:13`.
- Most of its forty operations are unused. A trial build with `dotio::math::Vec2` made an alias of `kinetra::Vec2` (89 errors in `kinetra_dotio_tests`, its logic and the benchmark) shows what Dotio actually uses beyond Kinetra:
  - `get_length` (37 uses), `get_normalized` (6), `rotate` (4), `get_direction` (2), `dot_product` and `determinant` (tests only): Kinetra has each as a free function (`length`, `normalized`, `rotated`, `atan2`, `dot`, `cross`).
  - `modulo` (1, `math/cell.cpp:39`) and one length comparison, `(point - enemy.position) < reach` (`ai/state.cpp:31`).
  - The aim math: `Vec2::intercept` and the three-argument `signed_angle_difference` (`ai/state.cpp:151` and `:243`).
  - Equality, `==` and `!=`: three places in the logic (`ai/state.cpp:42`, `entities/enemy.cpp:24` and `:34`) and about thirty checks in `dotio_tests.cpp`. `kinetra::Vec2` has no `operator==`; i4sim, Demo, Kinetra's own tests and the capture tool compare components instead (`labs/i4sim/tests/simulation_tests.cpp:1588`, `labs/demo/src/world/inspector.cpp:20`, `tools/capture/tests/capture_tests.cpp:667`).
  - Unused: the scalar `+`, `-`, `+=`, `-=`, the component product and quotient, `n / v`, the length comparisons other than `<`, `round`, `ceil`, `floor` and their copies, `set`, `translate`, `get_abs`, `get_difference`, `get_distance`, `get_signed`, `normalize` and `Segment::get_slope`.
- The Dotio README's open points name the type, and `get_signed`, which returns absolute values; the latter is unused and goes with the type.

## Goals and non-goals

- Goal: Dotio's logic, presentation, tests, benchmark and the capture tool use `kinetra::Vec2`, `kinetra::Segment` and `kinetra::Color`; `dotio/math/vector.h` and `src/math/vector.cpp` are gone, with all seven aliases.
- Goal: no conversions between two vector types remain; `to_world` and `from_world` stay, because they change unit and axis.
- Goal: behavior does not change: the golden fingerprint `0x9d126f5dc5068339` and every other Dotio and capture test hold unchanged, apart from the vector test that checked Dotio's own wrappers.
- Non-goal: `GridEntry` and its three aliases (`GridSize`, `GridPosition`, `GridCoordinates`); they are grid cells, not vectors, and Kinetra has no cell type to replace them with.
- Non-goal: any other open point of the Dotio README, such as `Tracker::add_instance`.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Equality of two vectors | `kinetra::Vec2` gets a defaulted `operator==` in Kinetra (branch `feat/vec2-equality`), which merges first | *User, 2026-11-24, against the agent's recommendation* of component comparisons in Lab only. Dotio compares vectors in three places of its logic and about thirty test checks; i4sim, Demo, the capture tool and Kinetra's own tests write `a.x == b.x && a.y == b.y` today, and can use `==` later. Costs a Kinetra pull request and the merge order Kinetra before Lab |
| Name at call sites | `kinetra::Vec2` everywhere | *Agent, confirmed by the user.* Dotio already writes Kinetra's types in full (`kinetra::Vec2` in `Commands`, `kinetra::Color` in `state_color`); a remaining `dotio::math::Vec2` alias would keep a second name for Kinetra's type |
| Segment and color | `kinetra::Segment` and `kinetra::Color` | *Agent, confirmed by the user.* Both are the same fields as Dotio's `Segment` (whose constructors and unused `get_slope` go) and `RGBA`; Dotio's logic already includes `kinetra/view/` headers, which are header-only and headless |
| Aim math (`intercept`, angle from a heading to a target) | Free functions in `ai/state.cpp`, the only user | *Agent, confirmed by the user.* They are a Dotio aiming rule, not vector math; the bodies stay the same |
| Unit safety between legacy pixels and world units | Carried by names, not types | *Agent, confirmed by the user.* Today a legacy position and a world point are different types, so the compiler rejects one where the other belongs. After the change both are `kinetra::Vec2`; `to_world`, `from_world`, the `Commands` comments ("in world units (Y up)") and the README's *Legacy pixels* row carry the distinction. The world-to-legacy boundary is small: `Application`, the inspector, the session and the capture tool's player. After the review, the contracts of `to_world`, `from_world` and `teleport_clear` name their unit |

## Design

- **The type.** Every `dotio::math::Vec2` and alias becomes `kinetra::Vec2`, written in full everywhere. `dotio::math::Segment` becomes `kinetra::Segment`, `dotio::math::RGBA` becomes `kinetra::Color`. `dotio/math/vector.h` and `src/math/vector.cpp` are removed; `math/` keeps `cell.h` and `random.h`.
- **Operations.** Each member becomes the Kinetra function it already called: `v.get_length()` is `kinetra::length(v)`, `get_normalized` is `kinetra::normalized`, `rotate` is `v = kinetra::rotated(v, angle)`, `get_direction` is `kinetra::atan2(v.y, v.x)`, `dot_product` and `determinant` are `kinetra::dot` and `kinetra::cross`. `modulo` becomes `std::fmod` per axis in `cell.cpp`, and `(a - b) < reach` becomes `kinetra::length(a - b) < reach`. Because `vector.cpp` already delegated to these functions, every result is the same float as before.
- **Aim math.** `intercept` and the three-argument `signed_angle_difference` move into `ai/state.cpp`'s anonymous namespace: `intercept()` keeps its body, and `aimed_arrow()` holds the aiming that `AttackState::update` and `KamikazeState::update` repeated line for line, with the same operations in the same order.
- **Adapters.** `physics_vector`, `legacy_vector`, `planner_point` and `route_of` disappear; `physics_body` and `apply_body` copy the fields directly, and `Route` takes the planner's points as they come. `to_world` and `from_world` stay in `application.h`, now with contracts that name both units.
- **Equality.** Kinetra's `Vec2` gets `constexpr bool operator==(const Vec2 &) const = default;`, so `!=` follows in C++20. It compares by IEEE rules, as Dotio's `operator==` did (`x == other.x && y == other.y`): +0 equals -0 and NaN equals nothing. Dotio's three comparisons in the logic and its test checks stay as they are; after the review, the presentation's interpolation guard, its tests and the behavior benchmark compare with `==` and `!=` too, instead of component by component.
- **Construction.** `kinetra::Vec2` is an aggregate, while Dotio's type had an `explicit Vec2(float s)` for (s, s). A single-argument brace initializer would now set only `x`. A search for one-argument constructions of every alias finds only copies (`Vec2{position - origin}`, `Vector2 next{position + velocity}`), so no call changes meaning; the compiler flags narrowing in braces.

## Affected areas

- `labs/dotio/include/dotio/` and `labs/dotio/src/`: nearly every header and source of the logic; `presentation/presentation.cpp` and `renderer.h` for `Segment` and `Color`; `math/cell.h` includes `<cstdint>` itself, which it took from `vector.h` before.
- `labs/dotio/tests/`: `dotio_tests.cpp`, `presentation_tests.cpp`, `behavior_benchmark.cpp`.
- `tools/capture/`: `dotio_player.h`, `dotio_player.cpp`, `tests/capture_tests.cpp`.
- `labs/dotio/CMakeLists.txt`: `src/math/vector.cpp` leaves `kinetra_dotio_logic`.
- Kinetra: `include/kinetra/simulation/math.h` (`operator==`), `tests/simulation_tests.cpp` (its test), `docs/engine/math-physics.md` (the contract row); the `external/Kinetra` pointer follows the Kinetra branch.
- Docs: Dotio README (layout, `math/` table, open points, the fingerprint paragraph, the case counts of its test table), the roadmap in both repositories.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| `kinetra::Vec2` compares equal exactly when both components do, including +0 and -0, and never for NaN | Kinetra's `vectors are equal exactly when both components compare equal` (`kinetra_tests`), with run-time inputs and a `static_assert` for the constant case | Targeted in Kinetra |
| No enemy decision, route or random draw changes | `a seeded game reproduces its recorded fingerprint` keeps `0x9d126f5dc5068339` after every commit | Targeted |
| Every other Dotio behavior | the whole `kinetra_dotio_tests` suite, unchanged apart from the vector test below | Targeted |
| The legacy/world conversion keeps its contract | `legacy vector math and coordinate conversion keep their contracts` becomes `legacy and world coordinates convert both ways`: it checks `to_world` and `from_world` on exact values in each direction and the round trip; its checks of Dotio's own wrappers go, since Kinetra's tests cover `length`, `dot`, `cross`, `normalized`, `rotated` and `signed_angle` | Targeted |
| The capture tool's Dotio player aims and leads as before | `kinetra_capture_tests` | Targeted |
| Behavior benchmark builds and runs | `kinetra_dotio_behavior_benchmark` and its CTest smoke run | Targeted |
| Presentation draws as before | `kinetra_dotio_presentation_tests` and the smoke run of `kinetra_dotio`, in CI's `ci` job | Full |

The change touches `presentation/` and `tools/capture/`, so the whole branch ends at the Full level: CI.

## Review

`/code-review` at medium depth on the branch, against the deleted `vector.cpp`:

| Finding | Resolution |
| --- | --- |
| No correctness finding: the aim math, `Player::shoot`, `get_cell_position` and every dropped conversion give the same floats as before, and no call builds a vector from one number | None needed |
| *Nit.* `teleport_clear(kinetra::Vec2 point, float radius)` does not say its unit, next to `select_enemy`, which takes a world point of the same type; before, the compiler rejected that mix-up | Its contract, and those of `to_world` and `from_world`, name the unit (675e695) |
| *Nit.* The renderer copied a `kinetra::Color` field by field, a leftover of the conversion from `RGBA` | It passes the color through (675e695) |
| Two findings in commits already merged on `main` (`capture_media.sh` stills, the launcher's generated header), because the local `main` was behind | Out of scope; `main` updated for the next diffs |

Agent review, round 1 (Kinetra#52 *Ready to merge* on `38a08f2`; Kinetra-Lab#114 *Fix first* on `07850e4`):

| Finding | Resolution |
| --- | --- |
| *Should fix* (#114). The open points and both *Follow-up* sections reach neither roadmap, so they would be lost once this document is history | An item *Vec2 equality in place of component comparisons* in each roadmap (Kinetra `b86a2ed`, followed by the pointer in 38e39e2); the `GridEntry` aliases, which no one intends to change, move to the Dotio README's open points |
| *Nit* (#52). The same follow-up for Kinetra's tests appeared only here | Kinetra's roadmap item above |
| *Nit* (#114). The Dotio README counted 44 cases in `kinetra_dotio` | Corrected: 46, and 5 in `kinetra_dotio_graphics`, not 3; the earlier open point's 45 was a miscount of CTest's list |
| *Nit* (#114). Six field-by-field copies of a `kinetra::Vec2` into a `kinetra::Vec2` remained (`inspector.cpp`, `grid.cpp`, `route.cpp`, `behavior_benchmark.cpp`) | The vectors are passed as they are (e36a3e9). Dotio's remaining component comparisons, in the presentation's interpolation guard, its tests and the benchmark, use `==` and `!=` (08b6e92) |

## Verification

Local, on Ubuntu with GCC 13 and clang-format 23.1.5; Kinetra at `38a08f2` (`feat/vec2-equality`):

| Command | Result |
| --- | --- |
| Kinetra: `cmake --build --preset dev`, `ctest --preset dev` | 279 of 279 passed; the new equality case with 7 assertions |
| A trial build with `dotio::math::Vec2` as an alias of `kinetra::Vec2`, before any change | 89 errors, the list in [Problem](#problem) |
| `ctest --preset dev -R fingerprint` after each code commit | `0x9d126f5dc5068339` each time |
| `cmake --build --preset dev --target kinetra_dotio_tests kinetra_dotio_behavior_benchmark kinetra_capture_tests kinetra_dotio_presentation kinetra_dotio kinetra_capture kinetra`, `ctest --preset dev -R '^kinetra_dotio:\|^kinetra_capture\|dotio_behavior' -LE soak` | No warning; 81 of 81 passed after each code commit, 80 after the review fixes without the benchmark |
| `cmake --preset graphics`, build of `kinetra_dotio_presentation_tests kinetra_dotio kinetra kinetra_capture`, `ctest --preset graphics -L graphics -R 'kinetra_dotio_graphics\|kinetra_smoke:dotio\|kinetra_capture_smoke'` | 7 of 7 passed; 6 of 6 again after the review fixes |
| `cmake --build --preset dev` (all of Lab against the new Kinetra commit), `ctest --preset dev -LE soak -j4` | No error or warning; 319 of 319 passed |
| `clang-format --dry-run --Werror` on every changed source | Clean |
| `python3 external/Kinetra/tools/check_docs.py` | All links resolve |
| After the review round, Kinetra at `b86a2ed`: `ctest --preset dev -R '^kinetra_dotio:\|^kinetra_capture\|dotio_behavior' -LE soak`, `ctest --preset graphics -L graphics -R 'kinetra_dotio_graphics\|kinetra_smoke:dotio'` | 81 of 81 and 6 of 6 passed; fingerprint unchanged |
| `python3 external/Kinetra/tools/check_copies.py` with the removed names (`legacy vector`, `get_signed`, `physics_vector`, `legacy_vector`, `planner_point`, `dotio::math::Vec2`, `math/vector.h`, `dotio::math::RGBA`, `dotio::math::Segment`, `Point2`, `signed_angle_difference`, `get_normalized`, `get_length`) | Only this document's index row and the word "Positions" in `docs/architecture.md` |

The Full level is the CI of both pull requests.

## Pull request

Two pull requests: Kinetra's adds `==` and merges first, with a merge commit; Lab's follows its branch commit.

### Kinetra

Title: `feat(simulation): compare Vec2 values with ==`

**Summary.** `kinetra::Vec2` gets a defaulted `operator==` (and so `!=`): exact equality of both components by IEEE rules. Kinetra-Lab's Dotio needs it to give up its own vector type, and every caller that writes `a.x == b.x && a.y == b.y` today can use it.

**Changes.**
- `include/kinetra/simulation/math.h`: `constexpr bool operator==(const Vec2 &) const = default;`, with a contract that +0 equals -0, NaN equals nothing, and arithmetic results that may round differently want a tolerance.
- `tests/simulation_tests.cpp`: `vectors are equal exactly when both components compare equal`, on run-time inputs and with a `static_assert` for the constant case.
- `docs/engine/math-physics.md`: the contract row of `math.h` names exact equality.
- `docs/roadmap.md`: a *Later* item, *Vec2 equality in place of component comparisons*, for Kinetra's own `same()` in `physics.cpp` and `tests/runtime_tests.cpp:250`, linked to Lab's item of the same name.

**Design.** The change has one design document, in Kinetra-Lab, where most of it is: `docs/design/2026-11-24-dotio-on-kinetra-vec2.md` on the Lab branch `ccr-5f0ec773-qklel2`. The user chose `==` in Kinetra over component comparisons in Lab only.

**Verification.** `cmake --build --preset dev` and `ctest --preset dev`: 279 of 279 passed, the new case with 7 assertions; clang-format and the docs check clean. Kinetra-Lab built against this commit: all of Lab in its `dev` preset with 319 of 319 tests passing, and Dotio's presentation tests, Dotio smoke and capture smoke in `graphics` (7 of 7).

**Review notes.** No existing expression changes meaning: a comparison of two `Vec2` did not compile before.

**Dependencies and merge order.** Merges before [SMB-M87/Kinetra-Lab#114](https://github.com/SMB-M87/Kinetra-Lab/pull/114), *refactor(dotio): use kinetra::Vec2*, with a merge commit, so the Lab pointer stays reachable.

**Follow-up.** The roadmap item *Vec2 equality in place of component comparisons*, here and in Kinetra-Lab.

### Kinetra-Lab

Title: `refactor(dotio): use kinetra::Vec2`

**Summary.** Dotio gives up its own vector type: `dotio::math::Vec2`, its seven aliases, its `Segment` and `RGBA` become `kinetra::Vec2`, `kinetra::Segment` and `kinetra::Color`, and `dotio/math/vector.h` is gone. The game does not change: the golden fingerprint keeps `0x9d126f5dc5068339`. Closes the roadmap item *Dotio on `kinetra::Vec2`*.

**Changes.**
- Dotio: the members it still used become the Kinetra functions they already called (`length`, `normalized`, `rotated`, `atan2`, `std::fmod` for the cell remainder); the aim math moves into `ai/state.cpp` as `intercept()` and `aimed_arrow()`, which also removes the aiming that two states repeated; about thirty unused operators and helpers go, `get_signed` among them.
- Dotio: the identity conversions `physics_vector`, `legacy_vector`, `planner_point` and `route_of` go; `to_world`, `from_world` and `teleport_clear` name their unit, because legacy pixels and world units now share a type.
- Dotio tests: the test of Dotio's vector wrappers becomes `legacy and world coordinates convert both ways`; the other checks are unchanged and compare with Kinetra's new `==`, as do the presentation tests, the presentation's interpolation guard and the behavior benchmark, which compared components before. No vector is copied field by field into another any more.
- Capture tool: its Dotio player and tests follow the rename.
- Docs: Dotio README (concepts, layout, `math/`, units, fingerprint, the case counts of its test table, open points), the design document; the roadmap item removed, and *Vec2 equality in place of component comparisons* added for i4sim, Demo and the capture tool.
- `external/Kinetra`: pointer to the head of Kinetra's `feat/vec2-equality`, with `operator==` and its roadmap item.

**Design.** This document. The user chose `==` in Kinetra over component comparisons in Lab only; unit safety between legacy pixels and world units moves from types to names.

**Verification.** See [Verification](#verification): the fingerprint after every commit, `kinetra_dotio`, `kinetra_capture` and the behavior benchmark in `dev`, the Dotio presentation, Dotio smoke and capture smoke in `graphics`, and the whole Lab in `dev` against the new Kinetra commit; CI is the Full level.

**Review notes.** Commits: the design; the pointer; *make Vec2 an alias of kinetra::Vec2* (the only commit with changed expressions); *drop the conversions between identical vectors*; *use kinetra::Vec2, Segment and Color throughout* (a compiler-checked rename); *say which unit a vector holds where both meet* (fixes from the author's review); then the agent review's round 1: *pass vectors instead of copying fields*, *compare vectors with ==*, the pointer to Kinetra's roadmap item, and the docs. Reviewing them one by one is easier than the whole diff.

**Dependencies and merge order.** Kinetra pull request [SMB-M87/Kinetra#52](https://github.com/SMB-M87/Kinetra/pull/52), *feat(simulation): compare Vec2 values with ==*, merges first, with a merge commit; this one then merges with its pointer at the Kinetra branch commit; Kinetra's `lab-pointer` workflow opens the pointer-only pull request to the merge commit.

**Follow-up.** The roadmap item *Vec2 equality in place of component comparisons*, here and in Kinetra; `GridEntry` keeps three aliases, an open point of the Dotio README.

## Open points

- i4sim, Demo, the capture tool and Kinetra still compare vectors component by component (for example `labs/i4sim/tests/simulation_tests.cpp:1588`, `labs/demo/src/world/inspector.cpp:20`, `tools/capture/tests/capture_tests.cpp:667`, Kinetra's `same()` in `src/simulation/physics.cpp` and `tests/runtime_tests.cpp:250`); they can use `==` now. Roadmap item *Vec2 equality in place of component comparisons*, in each repository.
- `GridEntry` keeps its three aliases (`GridSize`, `GridPosition`, `GridCoordinates`); no one intends to change them, so they are an open point of the [Dotio README](../../labs/dotio/README.md#open-points), not a roadmap item.

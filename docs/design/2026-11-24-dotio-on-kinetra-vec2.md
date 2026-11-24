# Dotio on `kinetra::Vec2`

Status: accepted · Branch: `ccr-5f0ec773-qklel2`, Kinetra `feat/vec2-equality` · Related: roadmap item *Dotio on `kinetra::Vec2`*, [Dotio open points](../../labs/dotio/README.md#open-points), [Dotio restructuring, open points](2026-11-21-dotio-restructuring.md#open-points)

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
| Unit safety between legacy pixels and world units | Carried by names, not types | *Agent, confirmed by the user.* Today a legacy position and a world point are different types, so the compiler rejects one where the other belongs. After the change both are `kinetra::Vec2`; `to_world`, `from_world`, the `Commands` comments ("in world units (Y up)") and the README's *Legacy pixels* row carry the distinction. The world-to-legacy boundary is small: `Application`, the inspector, the session and the capture tool's player |

## Design

- **The type.** Every `dotio::math::Vec2` and alias becomes `kinetra::Vec2`. Headers name it in full; `.cpp` files may write `using kinetra::Vec2;` at namespace scope, as i4sim does. `dotio::math::Segment` becomes `kinetra::Segment`, `dotio::math::RGBA` becomes `kinetra::Color`. `dotio/math/vector.h` and `src/math/vector.cpp` are removed; `math/` keeps `cell.h` and `random.h`.
- **Operations.** Each member becomes the Kinetra function it already called: `v.get_length()` is `kinetra::length(v)`, `get_normalized` is `kinetra::normalized`, `rotate` is `v = kinetra::rotated(v, angle)`, `get_direction` is `kinetra::atan2(v.y, v.x)`, `dot_product` and `determinant` are `kinetra::dot` and `kinetra::cross`. `modulo` becomes `std::fmod` per axis in `cell.cpp`, and `(a - b) < reach` becomes `kinetra::length(a - b) < reach`. Because `vector.cpp` already delegated to these functions, every result is the same float as before.
- **Aim math.** `intercept` and the three-argument `signed_angle_difference` move into `ai/state.cpp`'s anonymous namespace with the same bodies.
- **Adapters.** `physics_vector`, `legacy_vector` and `planner_point` disappear; `physics_body` and `apply_body` copy the fields directly. `to_world` and `from_world` stay in `application.h`.
- **Equality.** Kinetra's `Vec2` gets `constexpr bool operator==(const Vec2 &) const = default;`, so `!=` follows in C++20. It compares by IEEE rules, as Dotio's `operator==` did (`x == other.x && y == other.y`): +0 equals -0 and NaN equals nothing. Dotio's three comparisons in the logic and its test checks stay as they are.
- **Construction.** `kinetra::Vec2` is an aggregate, while Dotio's type had an `explicit Vec2(float s)` for (s, s). A single-argument brace initializer would now set only `x`. A search for one-argument constructions of every alias finds only copies (`Vec2{position - origin}`, `Vector2 next{position + velocity}`), so no call changes meaning; the compiler flags narrowing in braces.

## Affected areas

- `labs/dotio/include/dotio/` and `labs/dotio/src/`: nearly every header and source of the logic; `presentation/presentation.cpp` and `renderer.h` for `Segment` and `Color`.
- `labs/dotio/tests/`: `dotio_tests.cpp`, `presentation_tests.cpp`, `behavior_benchmark.cpp`.
- `tools/capture/`: `dotio_player.h`, `dotio_player.cpp`, `tests/capture_tests.cpp`.
- `labs/dotio/CMakeLists.txt`: `src/math/vector.cpp` leaves `kinetra_dotio_logic`.
- Kinetra: `include/kinetra/simulation/math.h` (`operator==`), `tests/simulation_tests.cpp` (its test), `docs/engine/math-physics.md` (the contract row); the `external/Kinetra` pointer follows the Kinetra branch.
- Docs: Dotio README (layout, `math/` table, open points, the fingerprint paragraph), the roadmap.

## Test plan

| Behavior | Test | Level |
| --- | --- | --- |
| `kinetra::Vec2` compares equal exactly when both components do, including +0 and -0, and never for NaN | Kinetra's `vectors are equal exactly when both components compare equal` (`kinetra_tests`), with run-time inputs and a `static_assert` for the constant case | Targeted in Kinetra |
| No enemy decision, route or random draw changes | `a seeded game reproduces its recorded fingerprint` keeps `0x9d126f5dc5068339` after every commit | Targeted |
| Every other Dotio behavior | the whole `kinetra_dotio_tests` suite, unchanged apart from the vector test below | Targeted |
| The legacy/world conversion keeps its contract | `legacy vector math and coordinate conversion keep their contracts` keeps its `to_world`/`from_world` check; its checks of Dotio's own wrappers go, since Kinetra's tests cover `length`, `dot`, `cross`, `normalized`, `rotated` and `signed_angle` | Targeted |
| The capture tool's Dotio player aims and leads as before | `kinetra_capture_tests` | Targeted |
| Behavior benchmark builds and runs | `kinetra_dotio_behavior_benchmark` and its CTest smoke run | Targeted |
| Presentation draws as before | `kinetra_dotio_presentation_tests` and the smoke run of `kinetra_dotio`, in CI's `ci` job | Full |

The change touches `presentation/` and `tools/capture/`, so the whole branch ends at the Full level: CI.

## Review

*Filled in after the review.*

## Verification

*Filled in after verification.*

## Pull request

*Filled in before the pull request opens.*

## Open points

*None yet.*

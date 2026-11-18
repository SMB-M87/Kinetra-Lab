# Factory simulation architecture

The factory application lives in Lab and consumes reusable simulation capabilities from the editable Kinetra submodule.

## Table of contents

- [Ownership](#ownership)
- [Execution](#execution)
- [Compatibility and fidelity](#compatibility-and-fidelity)
- [Predictive passing](#predictive-passing)
- [Verification](#verification)

[Directory guide](README.md) · [Project overview](../README.md) · [Application guide](../labs/i4sim/README.md)

## Ownership

| C# reference component | C++ owner / implementation |
| --- | --- |
| Blueprint, MoverGroup, ProducerGroup, model tables | [Lab blueprint loader](../labs/i4sim/src/blueprint.cpp) |
| BasicSteering, BorderRepulsionRect, CollisionDetection, CollisionAvoidance, SeekAndArrival | [Kinetra priority steering](../external/Kinetra/src/simulation/priority_steering.cpp) |
| AStarHeatmap / graph topology and congestion | [Kinetra WeightedGrid](../external/Kinetra/include/kinetra/weighted_grid.h); Lab chooses occupancy and replan timing |
| Rectangle geometry, neighborhood queries | Existing Kinetra Body/contact and SpatialGrid APIs |
| Blocked route recovery | Existing Kinetra Navigation with a snapshot of blocked grid cells and neighboring bodies |
| Dummy product actors, allocation managers, producer queues | [Lab deterministic simulation](../labs/i4sim/src/simulation.cpp) |
| Parking and producer approach policy | Lab reservations, staging and queue ownership |
| LoadingScreen / rendering / interaction | [Lab session](../labs/i4sim/src/session.cpp), shared Kinetra LaunchMenu, Runtime and bitmap text |
| Tick / render cycles | Existing Kinetra FixedClock and session loop; Lab chooses UPS |
| Dumps / metrics | Lab summary export |
| MQTT / actor framework / Windows platform | Excluded |

```mermaid
flowchart TD
    Launcher[Lab launcher] --> Screen[i4sim Loadscreen]
    Screen --> Blueprint[Lab JSON validation and group expansion]
    Blueprint --> Simulation[Lab products, movers and producers]
    Simulation --> Grid[Kinetra weighted grid and spatial queries]
    Simulation --> Steering[Kinetra priority steering]
    Simulation --> Recovery[Kinetra geometric Navigation]
    Simulation --> Physics[Kinetra contacts and collision resolution]
    Simulation --> Summary[Lab metrics export]
    Simulation --> Presentation[Lab inspection and overlays]
    Presentation --> Runtime[Kinetra Runtime]
```

Kinetra never imports factory models or Lab headers. Priority steering is a separate engine mode next to `steer` and `Navigation`, so Demo keeps its own tuning. The session uses the engine's right-button input state, and draws its Loadscreen with the optional heading and status text of `LaunchMenu`.

## Execution

A fixed tick generates bounded local workload, refreshes occupancy, moves agents using a consistent neighbor snapshot, and advances products through waiting, transport, queue and processing states. Recipes retain their mover until completion. A producer reserves a bounded queue slot before transport starts. Queue heads own the processing position; other reservations wait outside it. Consecutive supported interactions retain the same station reservation. Other completed interactions release the processor and clear its approach area. Blocked producer/mover controls retain products for resumption or retry; no network services participate.

Grid occupancy uses the original four-corner contributions of 4/8/16 per mover according to its size relative to a cell. The occupancy includes the querying mover itself, so high local weight alone does not trigger replanning. After 100 ticks without progress, weighted A* reconsiders the route; progress is measured along the remaining polyline rather than straight-line distance to the destination. Optional route recovery uses the existing finer geometric planner around actual bodies; it is disabled by default. Steering profiles never switch automatically. Improved prediction, original priority steering, experimental tangent/spring steering and experimental Reciprocal avoidance are selected explicitly. The local priority is overlap safety, predictive avoidance, then seek/arrival, followed by wall-normal velocity constraints; acceleration and speed are capped in original tick units. Lab's default lookahead is 32 ticks. Improved steering opts into exact rectangular clearance for axis-aligned mover pairs; the engine's conservative circular envelope remains the default for existing consumers. Parked/processing movers have zero prediction velocity and are fixed during mover-pair contact correction, so passing traffic cannot displace their reservations.

Reciprocal adapts the shared [avoidance controller](../external/Kinetra/include/kinetra/avoidance.h) to millimeters per tick with `dt = 1`. It reads an immutable snapshot; parked and blocked neighbors are noncooperative. Pairs use exact fixed axis-aligned rectangular envelopes with 2 mm clearance per axis; mixed shapes in the shared controller retain enclosing circles. Improved remains the default while factory throughput and contact behavior are evaluated. The last movement tick's `AVOID LIMIT` counter includes infeasible constraints and subsequent contact/border overrides. Existing wall constraints and rectangular contact correction still apply; reciprocal feasibility alone does not certify the final command or guarantee progress.

Optional `MotionSettings::parking_traffic` is application-owned parking coordination,
independent of the steering profile and disabled by default. Before movement, an
automatic return whose parking footprint intersects an active non-parking route
is redirected to a free staging cell outside protected routes. Deferred movers
retain their own parking reservation and cannot be allocated to a new product.
The original parking goal resumes when the conflict clears. New production or
manual goals cancel deferral; blocked departures stop protecting their route.
Disabling the option restores deferred parking goals without teleporting bodies.
Staging changes goals, while the existing steering and contact paths still govern
all movement. Decisions use stable mover order and are retried each tick; waiting
counts are exposed as `PARK WAIT`. Already parked, blocked and manually commanded
movers are not relocated. If no staging cell is available, the existing goal and
local avoidance remain active. This is a parking interlock, not intersection
reservation, corridor scheduling or a guarantee against starvation/deadlock.

Optional `MotionSettings::junction_traffic` detects isolated T/+ cells whose open
cardinal neighbors are degree-two corridor cells. It uses the shared
[`TrafficGate`](../external/Kinetra/include/kinetra/traffic.h), with one gate per
junction and spatially indexed candidate queries. Movers request their next zone
within a bounded route lookahead. A braking-travel check retains approaching
requests after goal changes until coasting can no longer enter the zone. Complete physical-occupancy snapshots include
parked and blocked movers. Non-admitted approaches get temporary stop lines enforced
by the existing acceleration-bounded border controller; they appear noncooperative
to reciprocal avoidance. Occupants may always exit. Goals are preserved and all
routes are prepared before the admission snapshot when junction coordination is
active. With no detected junctions, route preparation retains its original order.

The option is off by default. `CROSS WAIT` exposes waiting movers and detected zone
count. `EXIT` counts requests/approaches held for exit space. The boundary layer
outlines junctions in cyan and admitted exit pockets in lime. Disabling clears
logical gates, reservations and waits without moving bodies. A stopped occupant
continues blocking admission; there is no expiry timer.

Before a new grant, i4sim follows the route to the first fully clear exit and
reserves its swept rectangular footprint plus a bounded braking pocket (clipped
to the next waypoint). Static geometry, other bodies, their braking travel and
existing exit reservations must leave this pocket clear. A goal ending inside the
junction supplies no exit and is ineligible for a new grant. The oldest eligible
request may pass a blocked request, whose FIFO ticket remains intact. An admitted
exit stays reserved while that agent still requests or physically occupies the
zone, including after a goal change; it is not revoked by later readiness changes.
Other approaches get bounded stop-line constraints outside that pocket. Bodies
already inside a pocket or junction may leave freely. Physical queries reuse the
movement snapshot's spatial index.

This reserves space to clear the junction, not the entire outbound route. Existing
intrusions, infeasible braking after late activation, opposing single-lane traffic,
adjacent intersection complexes and permanently occupied zones still require
additional policy; this is not a general liveness or collision-free guarantee.

Optional `MotionSettings::corridor_traffic` applies the same gate/exit machinery to
whole straight single-cell corridors. The detector collects maximal horizontal or
vertical runs bounded on both sides by blocked cells, up to 256 cells long. Both
ends must open into a free three-cell-wide, two-cell-deep room. Dead ends, bends,
complex branches and longer runs are excluded. Zones store their actual rectangular
bounds and axis; existing junction APIs keep their original indexing.

Corridor admission is exclusive for the entire run. Waiting approaches stop far
enough into the entrance rooms to leave a full mover-length exit pocket plus
braking space free at either end, including when a parked occupant has no departure
route yet. The holder's actual exit is reserved and protected through the existing
readiness checks and bounded stop-line controller. After it fully clears the
corridor, the next request may enter; local reciprocal avoidance handles passing
in the wider room. Manual destinations and original parking reservations stay
intact. The application reuses the existing shared `TrafficGate` without an engine
API extension.

The option is disabled by default and lives under DEBUG → View → CORRIDOR TRAFFIC,
alongside scene/workload controls; the shorter Motion page retains its existing
rows. Blue outlines identify controlled corridors, lime outlines their reserved
exits, and `LANE WAIT` reports waiting movers/detected corridor count. Admission and
reservation state are cleared only for disabled policy types.

With exactly two active, unblocked opposing occupants, recovery may start after
both have at least 120 ticks without route progress and speed at most 0.05 mm/tick.
At most four candidates are checked: either mover reversing to either side of its
entrance room. Static footprint-aware routing and 2 mm peer/reservation clearance
must validate the complete retreat path and refuge against the current snapshot.
Refuges account for the mover's full rectangular footprint, including bodies longer
than a cell. The shortest valid path wins; equal costs prefer the higher stable ID.
Unsuccessful checks retry no more often than every 120 ticks.

The passing mover holds its initial position while the yielding mover follows a
private temporary route. Once settled in the refuge, the yielding mover waits until
the other fully clears the corridor. Both then resume ordinary control. Temporary
control targets retain original destinations, manual flags, task state and parking
reservations; movement still uses normal avoidance, border constraints and bounded
acceleration. Temporary targets cannot complete a production arrival. A new goal
cancels the affected recovery immediately; configuration rebuilds its routes, and
disabling corridor traffic clears recovery without moving bodies. Blocking a
participant suspends its motion and can be resumed. `RETREAT` counts active
recoveries, and pink path arrows show the current temporary steering target.

The mechanism does not relocate permanent blockers, handle three or more opposing
occupants, coordinate branched networks, or guarantee starvation freedom. A refuge
is checked when the manoeuvre is selected; later intrusions can still delay it via
local avoidance. Without a clear path/refuge, logical admission stays protected
and no forced escape or teleportation is attempted.

Weighted routes validate the mover's rectangular footprint on every swept link,
including exact start and destination coordinates. Contiguous wall segments are
merged so raster seams do not create artificial corners. Wall constraints bound
approach speed and penetration recovery without replacing steering along a wall;
all profiles retain their goal braking. Debug border vectors show the applied
acceleration correction. Final arrival requires position error below 0.1 mm,
speed below 0.05 mm/tick and a clear snap to the reserved destination.

Parking routes whose cell-center approach crosses a stationary mover are refined
with a bounded visibility graph using the original grid waypoints and expanded
rectangular mover corners. The same refinement runs after sustained lack of
progress, including movers that have stalled for at least 100 ticks. This supplies
safe aisle approaches without a circular envelope excluding a tight parking slot.
Transport allocation also requires a clear departure route around stationary/stalled
bodies, so a reserved mover trapped behind parked neighbors is skipped in favor
of an available mover. Refinement considers current stationary/stalled bodies, preserves grid clearance
and falls back to the weighted route if no connection exists or the 256-node
budget is exceeded. Other moving traffic still uses local avoidance. This is
separate from optional geometric recovery and does not guarantee progress in
arbitrary crowds.

Route recovery converts millimeter geometry into units of one planner resolution (a quarter of the smaller factory cell dimension), then converts the route back to millimeters. This keeps long factory routes within the planner's spatial query budget while retaining the configured resolution and mover clearance.

The loader accepts case-insensitive property names and string/numeric IDs, expands original groups, checks model names, nonnegative integer fields, finite dimensions, queue bounds and duplicate model-qualified IDs. Inputs are limited to 8 MiB, 10000 movers/producers and one million grid cells. Error messages include the source path. H/Madrid stations intentionally extend beyond the navigable world; their processing rectangles remain inside it.

## Compatibility and fidelity

The [application guide](../labs/i4sim/README.md#provenance-and-differences) is authoritative for intentional differences. This is a local C++ implementation of the case, not a binary-compatible replacement for actor/MQTT contracts. Numerical trajectories and asynchronous task ordering differ. Neither bit-for-bit behavioral parity nor identical throughput is established by the tests.

## Predictive passing

[Lab coordination](../labs/i4sim/src/predictive_passing.cpp) is an opt-in policy
for Improved and Reciprocal. It predicts conflicting, approximately opposite route
headings using preferred speed and a footprint-dependent lookahead. Detection uses
an immutable spatial snapshot; sorted encounter times and mover IDs make pairing
deterministic. Each mover can join one pair. Original destinations, product ownership
and manual commands remain intact; this policy overrides the current steering target.

Pairs commit to two stages: both reach stationary entry points in separate lateral
lanes, then both reach exit points beyond the encounter before replanning to their
original goals. Formation requires longitudinal separation of at least twice the
combined projected half extents plus 32 mm. Lane offsets use each rectangular
footprint plus 8 mm; two side choices and three shared lateral translations allow
passing near a wall. Temporary points use intermediate-waypoint braking. Both
participants must be within 0.1 mm and below 0.05 mm/tick before a stage completes.

Every formation/crossing sweep checks static grid clearance with 2 mm footprint
inflation, all other mover footprints in the snapshot, detected traffic zones, and
the swept envelopes of existing pairs. Detected narrow passages are excluded even
when their optional admission policy is off. Corridor recovery has precedence.
If no candidate fits, ordinary steering keeps its existing route.

Disabling the feature, blocking a participant or issuing a new goal releases the
pair and rebuilds the original routes. A pair also releases after 240 ticks without
measurable progress; both participants wait 120 ticks before retrying. Body positions
are never snapped by this policy. Existing local avoidance and contact correction
remain active. Snapshot checks do not reserve space against later intrusions by
unrelated movers; this is not a general collision-free or starvation-free scheduler.
The [regression and benchmark guide](../labs/i4sim/tests/README.md) describes coverage.

## Verification

See [i4sim tests](../labs/i4sim/tests/README.md), [Lab integration commands](../README.md#tests-and-resources) and [independent engine checks](../external/Kinetra/docs/development.md). Validation must include all original blueprints, complete recipes, a loaded factory under local workload, existing applications, graphics input, and the public engine consumer.

Contact episodes are counted per unordered mover pair or mover-boundary segment before physical correction. Pair/segment duration is tracked separately in ticks. The 0.001 mm artificial thickness of wall collision proxies is excluded; touching a wall without physical penetration is not a contact. Forces use a shared pre-step snapshot; contacts use the integrated snapshot and are resolved once per pair. Manual goals reserve free movers until arrival without interrupting other assignments.

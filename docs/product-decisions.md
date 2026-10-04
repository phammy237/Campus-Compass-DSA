# Product Decision Log

Decisions are inferred from the code and commit history; the repo has no written decision record from the time. "Why" entries are my reconstruction of reasoning, and the author should confirm them.

## Decision 01 — Keep schedule-builder data separate from the graded graph

### Context
The original program loads `data/edges.csv` and `data/classes.csv` and is tested against an assignment spec. The new feature needed a bigger campus and several sections per course.

### Options
1. Extend the existing CSVs.
2. Add separate synthetic data and a separate component.

### Decision
Option 2: `ScheduleBuilder` loads `data/synthetic_edges.csv` and `data/course_sections.csv` into its own `Graph`.

### Why
The header comment in `src/ScheduleBuilder.h` says it is "separate from the graded CampusCompass/Graph data so it can never affect the assignment's own tests."

### Tradeoffs
Gained: safe iteration, no regression risk to the graded behavior. Lost: two campus models that can disagree; builder results are demo-grade.

### Evidence
`src/ScheduleBuilder.h`, `CMakeLists.txt` (separate sources), UI disclaimer in `web/index.html` ("Synthetic data...").

## Decision 02 — Optimize over sections, not courses

### Context
A student's actual choice is "which section of each course," and sections differ in time and building.

### Options
1. Greedy: pick the nearest section per course.
2. Exhaustive search over one section per course, rejecting overlaps.

### Decision
Backtracking search that skips overlapping sections, orders chosen sections by start time, sums walking from home through each, and keeps the minimum (`ScheduleBuilder::search`). Dijkstra results are cached per source.

### Why
Greedy per-course choice ignores that a section's cost depends on the previous class's location. Exhaustive search finds the true minimum for the stated objective.

### Tradeoffs
Gained: optimal for total walking. Lost: runtime grows as the product of section counts (the app caps at 6 courses; the data has 31 courses / 95 sections). No weighting of other preferences. Tie-breaking is first-found.

### Evidence
`src/ScheduleBuilder.cpp` lines for `search`; `data/course_sections.csv`.

## Decision 03 — Report leave-by times, not just total minutes

### Context
Total walking minutes is hard to act on.

### Decision
Each leg carries `leaveByMinutes = startMinutes − travel` (`ScheduleLeg`), shown in a timeline after "Finalize Schedule."

### Why
It turns the optimization output into something a student can act on.

### Tradeoffs
No buffer: leave-by assumes arriving exactly at class start. First leg assumes starting from home at that time.

### Evidence
`src/ScheduleBuilder.h` (`ScheduleLeg`), `web/app.js` timeline rendering.

## Decision 04 — Real route geometry from OSRM; graph remains the source of times

### Context
The graph's edge weights are assignment data, not measured walking times, and draw as hops through buildings.

### Options
1. Draw the graph's path.
2. Ask a routing service for real geometry between waypoints.
3. Draw only the endpoints.

### Decision
The UI queries OSRM per leg for Walk/Bike/Drive profiles, caches results, and (per the comment in `web/app.js`) draws the endpoints only for the graph path because intermediate hops aren't geographically direct.

### Why
Lines on real streets look credible, and the stop list still shows what Dijkstra actually computed.

### Tradeoffs
Gained: readable map. Lost: displayed time and drawn route can disagree, especially in Bike/Drive; dependency on a public demo server with no SLA.

### Evidence
`web/app.js` (`TRAVEL_MODE_PROFILES`, `fetchWalkingLeg`, `routeEndpoints`); commit `7d28fbb`.

## Decision 05 — Heuristic straight-line fallback for suspicious walking detours

### Context
OSM campus footpaths are incomplete, so OSRM sometimes sends a short walk around via roads.

### Decision
In walk mode, if the leg is under 300 m and the routed distance is over 1.6x the straight-line distance, draw a straight line. On any routing failure, also draw a straight line.

### Why
A short leg with a big detour is more likely a map-data gap than a real obstacle (stated in the code comment).

### Tradeoffs
Gained: fewer absurd detours. Lost: can draw through real obstacles; thresholds unvalidated; failures are silent to the user.

### Evidence
`isSuspiciousDetour` in `web/app.js`.

## Decision 06 — Preserve a second interface (stdin commands) alongside the API

### Context
The graded program is a text command grammar (`insert`, `dropClass`, `printStudentZone`, ...).

### Decision
`CampusCompass::processCommand` stays the core; the API calls structured read-only queries (`shortestPath`, `getStudentInfo`, `studentZone`) and shares logic such as `buildZoneVertices` instead of parsing strings.

### Why
Keeps the tested behavior intact while allowing a UI.

### Tradeoffs
Two entry points to keep consistent. Student mutations through the API go via the command layer, so they inherit its validation.

### Evidence
`src/CampusCompass.h` comments; `src/ApiServer.cpp` routes.

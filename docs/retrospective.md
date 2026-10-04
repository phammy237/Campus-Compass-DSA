# Retrospective

## Original Goal
Per `docs/algorithms-report.md`, the starting point was a data-structures assignment: a command-driven campus graph with students, classes, shortest paths, connectivity and a minimum-cost "zone." The product extension (UI, API, Schedule Builder) was added afterward (commit `6d8bdad`).

## What We Shipped
- The graded engine with Catch2 tests.
- A local API and map UI with Explore, Students and Schedule Builder tabs.
- Walk/Bike/Drive display modes.

## What Worked
- Keeping the engine intact meant the UI could be added without breaking the tested behavior.
- Checking assumptions against real data surfaced two non-obvious facts: the sample graph has four components, and the induced-subgraph MST cost differs from the shortest-path-tree cost (27 vs 29).
- The Schedule Builder's search is small, readable and cached.

## What Didn't
- No user input shaped the features; there was no research.
- The builder runs on synthetic data, so its output can't be used for real planning.
- No tests for the builder or API.
- No screenshots or deployment, so the product is hard to evaluate without building it.

## Product Tradeoffs
- Total walking time as the only objective: simple and explainable, but ignores why students pick sections.
- Separate synthetic data: safe for the assignment, unrealistic for users.

## Technical Tradeoffs
- Exhaustive section search: optimal but exponential in the worst case.
- Public OSRM for geometry: no backend work, but an uncontrolled dependency.
- Path-compression-only union-find: documented as a conscious simplification in the algorithm report.

## What I Would Validate Next
- Do students actually pick different sections when shown travel cost?
- How far off are the leave-by times from real walking, and does a buffer matter?
- Would students rather see top 3 options than one?

## What I Would Build Differently
Start from a small real dataset (a few real sections and measured walking times) before building UI, add API tests alongside the endpoints, and decide up front how Bike/Drive should affect time, not just the drawn line.

*(Written from the repo's evidence and the existing report. The owner should add personal reflections.)*

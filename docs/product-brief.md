# Product Brief

Status: course project extended into a prototype. Everything about users and value below is hypothesis; the repo contains no user research.

## Problem

Students pick classes from a list of times and buildings, but the list does not say how long it takes to get between them. Whether a back-to-back schedule is physically possible is something students work out by hand or learn on the first day.

## Target User

A UF undergraduate choosing or checking a schedule, especially one starting from a distant residence hall. (Inferred from UFID validation in `src/Validation.cpp` and the UF building and residence data in `data/locations.csv`.)

## Current Alternatives

- Registrar course search: times and rooms, no travel cost.
- General map apps: distances, but unaware of the student's classes.
- Mental estimates and asking friends.

(Assumed alternatives; not researched.)

## Product Hypothesis

If a schedule tool treats classes as time windows on a map graph, students can see which schedules are feasible and pick sections that cut walking, before registering.

## Value Proposition

One place to answer "can I make it?" and "which sections should I pick?"

## MVP Scope (what exists)

- Shortest route between any two buildings, Walk/Bike/Drive display (`/api/shortest-path`).
- Student records with residence and 1-6 classes; schedule verification; per-student shortest routes and zone (`/api/students/*`).
- Road closures and campus connectivity check.
- Schedule Builder: course-level input, section-level recommendation, leave-by times (`/api/schedule-builder/recommend`).

## Non-Goals (current state)

- Real registrar data, accounts, persistence, or multi-user use.
- Registration itself.
- Accessibility routing, weather, crowd or bus timing.
- Optimizing anything other than total travel time (no preference for instructor, time of day, or gaps).

## Key Product Decisions

See [product-decisions.md](product-decisions.md).

## Risks & Assumptions

- **Data realism:** the builder runs on synthetic walking times and sections (stated in the UI). Recommendations are not trustworthy for real planning.
- **Assumed value:** no evidence students want this or would change section choices because of it.
- **Time model:** travel is treated as the only gap requirement; no allowance for class-change buffers, restroom stops, or lateness.
- **Dependency:** route drawing depends on a public OSRM server.
- **Mode mismatch:** Bike/Drive change the drawn route only; reported minutes come from the walking graph. To confirm: see `fetchWalkingLeg` in `web/app.js`.

## Success Criteria (proposed)

See [metrics.md](metrics.md). Minimum bar for calling the idea validated: students given a real schedule choose different sections, or catch an infeasible schedule, because of the tool.

## Future Opportunities

Real data import, buffer-time preferences, multi-objective scoring (walking vs. preferred times), and sharing a schedule. Prioritization in [roadmap.md](roadmap.md).

# Metrics Framework (all proposed)

**Nothing here was collected.** The prototype has no analytics, users or deployment. This is what I would instrument first.

## Product Goal
Help a student choose and verify a class schedule they can physically make.

## North Star Candidate
**Finalized schedules per active student per registration period.**
Why: finalizing means the student trusted the recommendation enough to commit. Measure: log the "Finalize Schedule" action (`#finalize-schedule-btn` in `web/index.html`) with a session ID.

## Acquisition
- Sessions that reach the Schedule Builder tab. Why: shows whether the feature is discoverable. Measure: tab-click events.

## Activation
- Share of sessions that request a recommendation within the first visit. Why: the core value moment. Measure: `POST /api/schedule-builder/recommend` with `found: true`.

## Engagement
- Recommendations requested per session (are students iterating on course lists?). Measure: API logs.
- Explore-tab route lookups per session.

## Retention
- Students returning in a later registration window. Needs accounts or persistent identity, which don't exist yet.

## Outcome Metrics
- Walking minutes saved versus the student's initial pick (needs a "before" schedule input that doesn't exist today).
- Share of recommended schedules kept unchanged after finalizing. Why: proxy for trust.
- Infeasible schedules caught: `verify-schedule` calls that return a failing leg.

## Guardrails
- "No schedule found" rate (`found: false`), which would show the data or search is too restrictive.
- Straight-line fallback rate and OSRM failure rate (`web/app.js` fallback branch).
- Recommended leave-by times vs. real measured walking time (needs real-world validation).

## Technical Health
- API latency of `recommend` as course count and sections grow (search is exponential in the worst case).
- Test coverage of `ScheduleBuilder` and `ApiServer` (currently none; `test/test.cpp` covers `CampusCompass` only).

# Roadmap

No release dates or commitments. This is my prioritization of the gaps visible in the repo.

## Now: make the existing claims trustworthy

| Item | Why it's here |
|---|---|
| Replace synthetic walking times with measured or routed ones | The builder's whole value depends on travel times; today they're synthetic. |
| Tests for `ScheduleBuilder` and API endpoints | Only `CampusCompass` is tested; the newest, most product-facing code isn't. |
| Show when the straight-line fallback is used | Users currently can't tell a real route from a fallback. |
| Persist students | In-memory state disappears on restart. |
| Add a screenshot/demo to the README | Nothing visual exists in the repo. |

## Next: strengthen the core value

| Item | Why it's here |
|---|---|
| Class-change buffer setting (e.g. leave-by plus N minutes) | Leave-by assumes arriving exactly on time, which is unrealistic. |
| Use Bike/Drive in the builder, not just map drawing | The mode toggle currently only affects route drawing. |
| Break ties and show alternatives (top 3 combos) | First-found tie-breaking hides equally good options. |
| Optional preferences (avoid early start, minimize gaps) | Walking-only optimization ignores other reasons students pick sections. |

## Later: bets that need validation first

| Item | Why it's here |
|---|---|
| Import real registrar section data | Highest realism gain, but needs a data source and permission. |
| Test with real students (5-10 sessions) | The product hypothesis is untested; do this before heavy investment. |
| Share/export a schedule | Only worth building if students finalize schedules at all. |

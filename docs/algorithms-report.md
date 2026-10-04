# Campus Compass — Design Report

## Notation

- **V** = number of vertices (locations) currently registered in `Graph::adjacency_`
- **E** = number of undirected edges loaded from `edges.csv`
- **S** = number of students currently stored in `StudentManager::students_`
- **C** = number of classes in the `classes_` catalog (loaded once from `classes.csv`, never mutated afterward)
- **k** = number of classes a given student has (`1 ≤ k ≤ 6` by the assignment's own constraint, so any `O(k)` or `O(k log k)` term below is really `O(1)` — it's written in terms of `k` to be honest about *why* it's constant, not just asserted)

All of `Graph`, `StudentManager`, and `CampusCompass::classes_` are backed by `std::unordered_map`/`std::unordered_set`. Their average-case operation cost is `O(1)`; the theoretical worst case (every key colliding into one bucket) is `O(n)` for that container. The complexities below state the average case, which is what actually governs behavior in practice, and call out the hash-collision worst case only where it changes the *dominant* term for a command.

## Per-command worst-case complexity

| Command | Complexity | Why |
|---|---|---|
| `insert` | `O(1)` average | Fixed number of tokens parsed (name + UFID + residence + count + ≤6 codes). Per code: `O(1)` avg. `classes_.find`, plus insertion into a `std::set<string>` for duplicate-detection (`O(log k)` = `O(1)` since `k ≤ 6`). One `O(1)` avg `students_` insert, one `O(1)` avg `graph_.addVertex`. |
| `remove` | `O(1)` average | One `unordered_map::erase`. |
| `dropClass` | `O(k)` = `O(1)` | `O(1)` avg map lookup, then a linear scan (`std::find`) over the student's own `classCodes` vector (≤6 entries) plus a `vector::erase` (≤6-element shift). |
| `replaceClass` | `O(k)` = `O(1)` | Same shape as `dropClass`: one map lookup for the student, one linear scan of their ≤6 codes, plus one `classes_.find` to confirm the new code exists in the catalog. |
| `removeClass` | `O(S·k)` = `O(S)` | The one command that isn't `O(1)`: `StudentManager::removeClass` has no index from class code → students, so it must iterate **every** currently-enrolled student and scan each one's ≤6-entry class list. Worst case scales with the number of enrolled students, not a constant. |
| `toggleEdgesClosure` | `O(deg(v))`, worst case `O(V)` | `Graph::toggleEdge` linearly scans `adjacency_[id1]` for the entry pointing at `id2`, then does the same on `adjacency_[id2]`'s list. A vertex's degree is bounded by `V−1`, so the worst case (a near-complete graph) is `O(V)`. |
| `checkEdgeStatus` | `O(deg(v))`, worst case `O(V)` | Same linear scan as above, one-directional. |
| `isConnected` | `O(V + E)` | One BFS from an arbitrary vertex over open edges. Each undirected edge is stored as two directed `Edge` entries, so the traversal visits `O(V + 2E) = O(V + E)` entries total. |
| `printShortestEdges` | `O((V + E) log V)` | Dominated by one binary-heap Dijkstra run (see below). Sorting the student's ≤6 class codes lexicographically is `O(k log k) = O(1)`. |
| `printStudentZone` | `O((V + E) log V)` | One Dijkstra run (`O((V+E) log V)`), then up to `k ≤ 6` path reconstructions (`O(V)` each in the worst case, so `O(k·V) = O(V)` total), then `inducedMST` over the induced subgraph: collecting candidate edges is `O(E)`, sorting them is `O(E log E)`, and Kruskal's union-find loop is effectively `O(E log V)` (see MST notes below — path compression without union by rank gives amortized `O(log V)` per `find`, not full inverse-Ackermann). Since `E ≤ V²` gives `log E = O(log V)`, every term collapses into `O((V+E) log V)`. |
| `verifySchedule` | `O((V + E) log V)` | Sorting ≤6 classes by start time is `O(1)`. For each of the ≤5 consecutive pairs, a **fresh** Dijkstra run is triggered from that pair's earlier class location — `O(k·(V+E) log V)`, and since `k ≤ 6` is a constant factor, this is asymptotically the same class as `printShortestEdges`. |
| `ParseCSV` (startup, not a per-line command) | `O(V + E + C)` | One linear pass over each CSV file; each row does a fixed number of `stoi` calls and one `O(1)` avg map/vertex insert. |

## Dijkstra implementation

`Graph::dijkstra` (`src/Graph.cpp`) is a standard **binary-heap Dijkstra** using
`std::priority_queue<pair<int,int>, vector<pair<int,int>>, greater<>>` as a min-heap keyed on distance.

A few implementation details worth calling out because they're not the textbook-simplest version:

- **Lazy deletion instead of decrease-key.** `std::priority_queue` has no `decrease-key` operation. Instead of maintaining an indexed heap, the code pushes a *new* `(distance, vertex)` pair every time a shorter distance to `e.to` is found, and simply **skips** a popped vertex if it's already in `finalized` (line `if (finalized.count(u)) continue;`). This means the heap can hold stale entries, but each is discarded in `O(1)` when popped, so it doesn't change the asymptotic bound — at most `O(E)` pushes happen in total (one per edge relaxation), each costing `O(log E) = O(log V)` (since `E ≤ V²`), giving the standard `O((V + E) log V)`.
- **Closed edges are invisible, not filtered afterward.** The `if (!e.open) continue;` check inside the relaxation loop means a closed road is never even considered as a candidate — `toggleEdgesClosure` doesn't need to touch Dijkstra at all; the *next* Dijkstra call just naturally routes around it.
- **`dist`/`parent` are `unordered_map`s, not arrays.** Since location IDs from `edges.csv` are not contiguous (verified against the real sample data — ID `48` never appears at all), indexing by a dense array would be incorrect. Every vertex that's actually reachable gets an entry; an *absent* entry in `dist` is exactly how `-1`/unreachable is detected downstream, with no separate "visited" sentinel needed.

## MST implementation (`printStudentZone`)

`Graph::inducedMST` implements **Kruskal's algorithm** with union-find, deliberately *not* Prim's — and the choice matters here for a reason specific to this assignment, not just personal preference: the vertex set passed in is small and already filtered down to "vertices that appear on some shortest path from the residence," so the induced subgraph tends to be sparse. Kruskal's "sort edges, then union-find" shape is a natural fit for a subgraph that's handed to you as an edge list, without needing to build an adjacency-based priority queue over the whole original graph.

Two details worth being precise about, because a naive implementation of this exact step is the easiest place to get this assignment silently wrong:

1. **The candidate edge set is the induced subgraph, not the shortest-path tree.** `inducedMST` re-scans `adjacency_` for **every** open edge with both endpoints in the given vertex set — including edges that never appeared on anyone's shortest path. This was not a hypothetical concern: testing against the real sample data showed a student whose shortest-path-tree-only edges would sum to a cost of **29**, while the correct induced-subgraph MST is **27** — the induced subgraph contains a cheaper edge (`13–23`, weight 4) that Kruskal picks over a shortest-path-tree edge Prim-on-the-tree-alone would have been stuck with.
2. **Union-find here uses path compression only, not union by rank.** The `find` helper does path *halving* (`parent[x] = parent[parent[x]]`) but the union step (`parent[ru] = rv;`) always attaches the first root to the second arbitrarily — there's no size/rank comparison. Path compression alone still gives an amortized `O(log n)` per operation (a well-known but slightly weaker result than the `O(α(n))` bound that requires *both* heuristics together). Given the vertex sets here are small (bounded by how many vertices a shortest-path tree touches), this simplification doesn't matter in practice, but it's worth documenting as a conscious simplification rather than an oversight.

## What I learned

*(This section is written in first person as a starting draft — since it's meant to capture your own experience with this project, please read it over and edit it to reflect what you actually found notable, difficult, or interesting. I've grounded the draft in things we specifically ran into together, not generic statements.)*

Working through this project made the gap between "the algorithm works on paper" and "the algorithm is provably correct on *this* data" much more concrete. The clearest example was Phase 7: I initially assumed the student-zone cost was just the sum of shortest-path-tree edges, and it took closing the loop with an independent Python reimplementation to actually see the 29-vs-27 discrepancy and understand *why* the induced subgraph matters — it's not a corner case, it showed up immediately in the provided sample data. Similarly, I assumed the sample campus graph would be a single connected component, and instead discovered it's four separate components (a 49-vertex cluster plus three isolated pairs) — which meant `isConnected` returning `false` even before any closures wasn't a bug, it was the actual shape of the input.

I also got more comfortable with the tradeoffs behind `std::priority_queue`-based Dijkstra (lazy deletion vs. a proper decrease-key structure) and with the honest limits of a path-compression-only union-find, rather than reciting the textbook inverse-Ackermann bound without checking whether my code actually earns it.

## What I would change if starting over

*(Also a draft — personalize this.)*

- I'd consider adding a thin `Location` abstraction earlier (even just an `unordered_map<int, string>` for names from `edges.csv`) — we ended up deciding not to store `Name_1`/`Name_2` since nothing in the spec printed them, but if a later requirement had needed a location name, retrofitting it would have touched `ParseCSV`, `Graph`, and every place that already assumed "vertex = bare `int`."
- I'd write the closure-driven "reachable → closed → unreachable" test earlier in the process (it ended up in Phase 10, but it's exactly the kind of test that would have caught a mistake in Dijkstra's edge-open check immediately, rather than after the fact).
- I'd verify assumptions about the sample data (connectivity, ID contiguity) *before* designing the adjacency list representation, rather than assuming and getting lucky that `unordered_map<int, vector<Edge>>` was the right call from the start.

## Reflection on difficulty and intuition

*(Also a draft — personalize this.)*

The individual algorithms (Dijkstra, Kruskal, BFS) were the least difficult part — they're standard and well-documented. The harder part was the *coordination* logic: getting `insert`'s validation order right (format checks, uniqueness, catalog membership, duplicate detection, all needing to compose into a single pass/fail), and correctly deriving the student-zone vertex set from Dijkstra's output before handing it to Kruskal. The project rewarded checking assumptions against the actual provided data (contiguity, connectivity, exact time formats) rather than the assignment prose alone — several details that looked ambiguous in the written spec turned out to be unambiguous once I actually looked at `edges.csv`/`classes.csv` directly.

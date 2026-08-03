#include "Graph.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace {
Edge *findEdgeTo(vector<Edge> &edges, int to) {
    for (Edge &e : edges) {
        if (e.to == to) return &e;
    }
    return nullptr;
}

const Edge *findEdgeTo(const vector<Edge> &edges, int to) {
    for (const Edge &e : edges) {
        if (e.to == to) return &e;
    }
    return nullptr;
}
} // namespace

void Graph::addVertex(int id) {
    adjacency_.emplace(id, vector<Edge>{});
}

void Graph::addEdge(int id1, int id2, int time) {
    auto it1 = adjacency_.emplace(id1, vector<Edge>{}).first;
    auto it2 = adjacency_.emplace(id2, vector<Edge>{}).first;
    it1->second.push_back(Edge{id2, time, true});
    it2->second.push_back(Edge{id1, time, true});
}

bool Graph::hasVertex(int id) const {
    return adjacency_.count(id) > 0;
}

EdgeStatus Graph::getEdgeStatus(int id1, int id2) const {
    auto it = adjacency_.find(id1);
    if (it == adjacency_.end()) return EdgeStatus::DNE;
    const Edge *e = findEdgeTo(it->second, id2);
    if (!e) return EdgeStatus::DNE;
    return e->open ? EdgeStatus::Open : EdgeStatus::Closed;
}

bool Graph::toggleEdge(int id1, int id2) {
    auto it1 = adjacency_.find(id1);
    if (it1 == adjacency_.end()) return false;

    Edge *e1 = findEdgeTo(it1->second, id2);
    if (!e1) return false;
    e1->open = !e1->open;

    // undirected: flip the matching entry on the other side too
    Edge *e2 = findEdgeTo(adjacency_[id2], id1);
    if (e2) e2->open = !e2->open;
    return true;
}

bool Graph::isConnected(int id1, int id2) const {
    if (!hasVertex(id1) || !hasVertex(id2)) return false;

    unordered_set<int> visited;
    queue<int> toVisit;
    visited.insert(id1);
    toVisit.push(id1);

    while (!toVisit.empty()) {
        int current = toVisit.front();
        toVisit.pop();
        for (const Edge &e : adjacency_.at(current)) {
            if (e.open && visited.insert(e.to).second) {
                toVisit.push(e.to);
            }
        }
    }

    return visited.count(id2) > 0;
}

bool Graph::isConnected() const {
    if (adjacency_.empty()) return true;

    unordered_set<int> visited;
    queue<int> toVisit;
    int start = adjacency_.begin()->first;
    visited.insert(start);
    toVisit.push(start);

    while (!toVisit.empty()) {
        int current = toVisit.front();
        toVisit.pop();
        for (const Edge &e : adjacency_.at(current)) {
            if (e.open && visited.insert(e.to).second) {
                toVisit.push(e.to);
            }
        }
    }

    return visited.size() == adjacency_.size();
}

DijkstraResult Graph::dijkstra(int source) const {
    DijkstraResult result;
    if (!hasVertex(source)) return result;

    result.dist[source] = 0;
    using DistVertex = pair<int, int>; // (distance, vertex)
    priority_queue<DistVertex, vector<DistVertex>, greater<DistVertex>> pq;
    pq.push({0, source});

    unordered_set<int> finalized;
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (finalized.count(u)) continue;
        finalized.insert(u);

        for (const Edge &e : adjacency_.at(u)) {
            if (!e.open) continue;
            int newDist = d + e.time;
            auto it = result.dist.find(e.to);
            if (it == result.dist.end() || newDist < it->second) {
                result.dist[e.to] = newDist;
                result.parent[e.to] = u;
                pq.push({newDist, e.to});
            }
        }
    }
    return result;
}

vector<int> Graph::reconstructPath(const DijkstraResult &result, int source, int target) const {
    if (source == target) return {source};
    if (result.dist.find(target) == result.dist.end()) return {}; // unreachable

    vector<int> path;
    int current = target;
    while (current != source) {
        path.push_back(current);
        auto it = result.parent.find(current);
        if (it == result.parent.end()) return {}; // should not happen if target is reachable
        current = it->second;
    }
    path.push_back(source);
    reverse(path.begin(), path.end());
    return path;
}

namespace {
struct WeightedEdge {
    int u, v, time;
};

int find(unordered_map<int, int> &parent, int x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}
} // namespace

int Graph::inducedMST(const unordered_set<int> &vertices) const {
    // gather every open edge whose two endpoints are both in `vertices`,
    // counted once per undirected pair (not the union of shortest-path edges —
    // any direct open edge between two selected vertices is eligible)
    vector<WeightedEdge> edges;
    for (int v : vertices) {
        auto it = adjacency_.find(v);
        if (it == adjacency_.end()) continue;
        for (const Edge &e : it->second) {
            if (e.open && v < e.to && vertices.count(e.to) > 0) {
                edges.push_back({v, e.to, e.time});
            }
        }
    }
    sort(edges.begin(), edges.end(),
         [](const WeightedEdge &a, const WeightedEdge &b) { return a.time < b.time; });

    unordered_map<int, int> parent;
    for (int v : vertices) parent[v] = v;

    int totalCost = 0;
    size_t edgesUsed = 0;
    size_t edgesNeeded = vertices.empty() ? 0 : vertices.size() - 1;
    for (const WeightedEdge &e : edges) {
        if (edgesUsed == edgesNeeded) break;
        int ru = find(parent, e.u);
        int rv = find(parent, e.v);
        if (ru != rv) {
            parent[ru] = rv;
            totalCost += e.time;
            ++edgesUsed;
        }
    }
    return totalCost;
}

int Graph::studentZoneMST(const set<int> &vertices) const {
    unordered_set<int> asUnordered(vertices.begin(), vertices.end());
    return inducedMST(asUnordered);
}

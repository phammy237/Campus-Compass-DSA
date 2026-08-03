#pragma once

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Models.h"

using namespace std;

enum class EdgeStatus { Open, Closed, DNE };

struct DijkstraResult {
    unordered_map<int, int> dist;   // shortest time from source; unreachable vertices are absent
    unordered_map<int, int> parent; // predecessor on the shortest path; absent for source/unreachable
};

class Graph {
public:
    // registers a vertex even if it has no edges yet (e.g. an isolated location)
    void addVertex(int id);

    // undirected: adds id1->id2 and id2->id1, each with the given time, open by default
    void addEdge(int id1, int id2, int time);

    bool hasVertex(int id) const;

    EdgeStatus getEdgeStatus(int id1, int id2) const;

    // flips open/closed on both directions of the edge; returns false if the edge does not exist
    bool toggleEdge(int id1, int id2);

    // BFS reachability from id1 to id2 across open edges only;
    // false if either vertex is unknown
    bool isConnected(int id1, int id2) const;

    // BFS reachability across the whole graph: true iff every registered vertex
    // is reachable from an arbitrary start vertex over open edges (vacuously
    // true for an empty graph)
    bool isConnected() const;

    // Dijkstra over open edges only
    DijkstraResult dijkstra(int source) const;

    // walks DijkstraResult.parent from target back to source; empty if unreachable
    vector<int> reconstructPath(const DijkstraResult &result, int source, int target) const;

    // MST cost over the induced subgraph of open edges among `vertices` —
    // a generic graph operation; callers attach any domain meaning to the result
    int inducedMST(const unordered_set<int> &vertices) const;

    // convenience wrapper over inducedMST for callers holding a set<int>
    int studentZoneMST(const set<int> &vertices) const;

private:
    unordered_map<int, vector<Edge>> adjacency_;
};

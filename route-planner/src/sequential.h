// sequential.h - baseline: Dijkstra with a binary heap
#pragma once
#include "graph.h"

// Single-source shortest paths. If t >= 0 stops as soon as t is settled.
inline void dijkstra(const Graph& g, int s, int t, std::vector<ll>& dist, ll& settled) {
    typedef std::pair<ll, int> P;
    dist.assign(g.n, INF); settled = 0;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    dist[s] = 0; pq.push({0, s});
    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;            // stale heap entry
        settled++;
        if (u == t) break;
        for (int e = g.off[u]; e < g.off[u + 1]; e++) {
            int v = g.to[e]; ll nd = d + g.w[e];
            if (nd < dist[v]) { dist[v] = nd; pq.push({nd, v}); }
        }
    }
}

// Walk back from t over the reverse graph: predecessor p satisfies dist[p]+w == dist[u].
// Works for any algorithm that produced correct distances (no parent array needed).
inline std::vector<int> reconstruct(const Graph& g, const std::vector<ll>& dist, int s, int t) {
    std::vector<int> path;
    if (dist[t] >= INF) return path;
    int u = t; path.push_back(u);
    for (int steps = 0; u != s && steps <= g.n; steps++) {
        int nxt = -1;
        for (int e = g.roff[u]; e < g.roff[u + 1]; e++) {
            int p = g.rto[e];
            if (dist[p] < INF && dist[p] + g.rw[e] == dist[u]) { nxt = p; break; }
        }
        if (nxt < 0) break;
        u = nxt; path.push_back(u);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

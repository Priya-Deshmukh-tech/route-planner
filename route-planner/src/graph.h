// graph.h - CSR graph + DIMACS loader (shared by sequential and parallel code)
#pragma once
#include <bits/stdc++.h>
typedef long long ll;
static const ll INF = LLONG_MAX / 4;

struct Graph {
    int n = 0;
    std::vector<int> off, to, w;      // forward CSR
    std::vector<int> roff, rto, rw;   // reverse CSR (used for path reconstruction)
    std::vector<int> x, y;            // coordinates (micro-degrees: lon, lat)
    double avgw = 1;
    int center = 0;                   // node closest to the centroid of the map

    void build(int n_, const std::vector<std::array<int, 3>>& E) {
        n = n_;
        off.assign(n + 1, 0); roff.assign(n + 1, 0);
        double sum = 0;
        for (auto& e : E) { off[e[0] + 1]++; roff[e[1] + 1]++; sum += e[2]; }
        for (int i = 0; i < n; i++) { off[i + 1] += off[i]; roff[i + 1] += roff[i]; }
        to.resize(E.size()); w.resize(E.size()); rto.resize(E.size()); rw.resize(E.size());
        std::vector<int> c(off.begin(), off.end() - 1), rc(roff.begin(), roff.end() - 1);
        for (auto& e : E) {
            int p = c[e[0]]++; to[p] = e[1]; w[p] = e[2];
            int q = rc[e[1]]++; rto[q] = e[0]; rw[q] = e[2];
        }
        avgw = E.empty() ? 1 : sum / E.size();
    }
};

inline bool load_graph(const std::string& gr, const std::string& co, Graph& g) {
    FILE* f = fopen(gr.c_str(), "r");
    if (!f) return false;
    char line[256]; int n = 0;
    std::vector<std::array<int, 3>> E;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'p') { int m; sscanf(line, "p sp %d %d", &n, &m); E.reserve(m); }
        else if (line[0] == 'a') { int u, v, w; sscanf(line, "a %d %d %d", &u, &v, &w); E.push_back({u - 1, v - 1, w}); }
    }
    fclose(f);
    g.build(n, E);
    g.x.assign(n, 0); g.y.assign(n, 0);
    f = fopen(co.c_str(), "r");
    if (!f) return false;
    double sx = 0, sy = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'v') { int id, x, y; sscanf(line, "v %d %d %d", &id, &x, &y); g.x[id - 1] = x; g.y[id - 1] = y; sx += x; sy += y; }
    }
    fclose(f);
    sx /= n; sy /= n; double best = 1e30;
    for (int i = 0; i < n; i++) {
        double d = (g.x[i] - sx) * (g.x[i] - sx) + (g.y[i] - sy) * (g.y[i] - sy);
        if (d < best) { best = d; g.center = i; }
    }
    return true;
}

// Sub-network of the first `target` nodes reached by BFS from `center`
// (used to study how run time grows with network size). Source = new id 0.
inline Graph induced_ball(const Graph& g, int center, int target) {
    std::vector<int> id(g.n, -1), order; order.reserve(target);
    std::queue<int> q; q.push(center); id[center] = 0; order.push_back(center);
    while (!q.empty() && (int)order.size() < target) {
        int u = q.front(); q.pop();
        for (int e = g.off[u]; e < g.off[u + 1] && (int)order.size() < target; e++) {
            int v = g.to[e];
            if (id[v] < 0) { id[v] = order.size(); order.push_back(v); q.push(v); }
        }
    }
    std::vector<std::array<int, 3>> E;
    for (int u : order)
        for (int e = g.off[u]; e < g.off[u + 1]; e++)
            if (id[g.to[e]] >= 0) E.push_back({id[u], id[g.to[e]], g.w[e]});
    Graph s; s.build(order.size(), E);
    return s;
}

// parallel.h - OpenMP implementations
//  (1) delta-stepping SSSP : parallelism INSIDE one shortest-path query
//  (2) batch routing       : parallelism ACROSS many vehicles' queries
#pragma once
#include "sequential.h"
#include <atomic>
#include <omp.h>

static inline bool atomic_min(std::atomic<ll>& d, ll nd) {   // true if we lowered d
    ll cur = d.load(std::memory_order_relaxed);
    while (nd < cur)
        if (d.compare_exchange_weak(cur, nd, std::memory_order_relaxed)) return true;
    return false;
}

inline ll auto_delta(const Graph& g) { return std::max<ll>(1, (ll)(g.avgw * 2)); }

// Meyer & Sanders delta-stepping. Vertices are kept in buckets of width `delta`.
// Light edges (w<=delta) are relaxed repeatedly inside the current bucket,
// heavy edges once after the bucket is empty. Each relaxation round is an OpenMP loop.
inline void delta_stepping(const Graph& g, int s, int t, ll delta, int T,
                           std::vector<ll>& out, ll& relaxations) {
    const int n = g.n;
    std::unique_ptr<std::atomic<ll>[]> d(new std::atomic<ll>[n]);
    for (int i = 0; i < n; i++) d[i].store(INF, std::memory_order_relaxed);
    d[s].store(0);
    std::vector<std::vector<int>> buckets(1);
    buckets[0].push_back(s);
    std::vector<std::vector<int>> req(T), done(T);
    ll rel = 0; size_t cur = 0;

    auto place = [&](int v) {
        size_t b = (size_t)(d[v].load() / delta);
        if (b >= buckets.size()) buckets.resize(b + 1);
        buckets[b].push_back(v);
    };
    // One parallel relaxation round over `F`. heavy=false: light edges + record settled;
    // heavy=true: heavy edges only.
   
auto round = [&](const std::vector<int>& F, bool heavy) {
    for (int i = 0; i < T; i++) {
        req[i].clear();
        done[i].clear();
    }

    ll r = 0;

    #pragma omp parallel num_threads(T) reduction(+:r) if (T > 1 && F.size() >= 64 * T)
    {
        int id = omp_get_thread_num();
        auto& rq = req[id];
        auto& dn = done[id];

        #pragma omp for schedule(dynamic, 16)
        for (long k = 0; k < (long)F.size(); k++) {
            int u = F[k];
            ll du = d[u].load(std::memory_order_relaxed);

            if (!heavy) {
                if (size_t(du / delta) != cur) continue;
                dn.push_back(u);
            }

            for (int e = g.off[u]; e < g.off[u + 1]; e++) {
                bool isHeavy = g.w[e] > delta;
                if (isHeavy != heavy) continue;

                r++;

                if (atomic_min(d[g.to[e]], du + g.w[e]))
                    rq.push_back(g.to[e]);
            }
        }
    }

    rel += r;

    for (int i = 0; i < T; i++) {
        for (int v : req[i]) place(v);
    }
};

    while (true) {
        while (cur < buckets.size() && buckets[cur].empty()) cur++;
        if (cur >= buckets.size()) break;
        if (t >= 0) { ll dt = d[t].load(); if (dt < INF && (ll)cur * delta > dt) break; }  // early exit
        std::vector<int> R;
        while (!buckets[cur].empty()) {
            std::vector<int> F; F.swap(buckets[cur]);
            round(F, false);
            for (int i = 0; i < T; i++) R.insert(R.end(), done[i].begin(), done[i].end());
        }
        std::sort(R.begin(), R.end()); R.erase(std::unique(R.begin(), R.end()), R.end());
        round(R, true);
    }
    out.resize(n);
    for (int i = 0; i < n; i++) out[i] = d[i].load();
    relaxations = rel;
}

// Many (origin,destination) vehicle queries; each is an independent Dijkstra.
// Embarrassingly parallel: threads pick queries dynamically. T==1 is the sequential baseline.
inline double run_batch(const Graph& g, const std::vector<std::pair<int, int>>& P, int T,
                        std::vector<ll>& res, std::vector<std::vector<int>>* paths = nullptr) {
    res.assign(P.size(), -1);
    if (paths) paths->assign(P.size(), {});
    double t0 = omp_get_wtime();
    #pragma omp parallel for num_threads(T) schedule(dynamic, 1)
    for (long i = 0; i < (long)P.size(); i++) {
        std::vector<ll> dist; ll st;
        dijkstra(g, P[i].first, P[i].second, dist, st);
        if (dist[P[i].second] < INF) {
            res[i] = dist[P[i].second];
            if (paths) (*paths)[i] = reconstruct(g, dist, P[i].first, P[i].second);
        }
    }
    return (omp_get_wtime() - t0) * 1000.0;
}

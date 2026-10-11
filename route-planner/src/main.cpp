// main.cpp - command-line engine used by the web UI (server.py) and for batch experiments.
//   router <gr> <co> route  <dijkstra|delta> <threads> <src> <dst> [delta=0(auto)] [reps=3]
//   router <gr> <co> fleet  <seq|omp> <threads> <K> <seed>
//   router <gr> <co> bench  <sizes,..> <threads,..> <reps> <batchQ(0=off)> [delta=0]
//   router <gr> <co> verify <numQueries>
// Output: one JSON document on stdout.   Node ids are 0-based.
#include "parallel.h"
using namespace std;

static vector<int> csv(const string& s) {
    vector<int> r; stringstream ss(s); string t;
    while (getline(ss, t, ',')) if (!t.empty()) r.push_back(stoi(t));
    return r;
}
static void jpath(const vector<int>& p) {
    printf("["); for (size_t i = 0; i < p.size(); i++) printf(i ? ",%d" : "%d", p[i]); printf("]");
}
static vector<pair<int, int>> random_pairs(int n, int K, unsigned seed) {
    mt19937 rng(seed); vector<pair<int, int>> P;
    for (int i = 0; i < K; i++) { int a = rng() % n, b = rng() % n; if (a == b) b = (b + 1) % n; P.push_back({a, b}); }
    return P;
}

static int cmd_route(const Graph& g, int argc, char** argv) {
    string algo = argv[4]; int T = atoi(argv[5]), s = atoi(argv[6]), t = atoi(argv[7]);
    ll delta = argc > 8 ? atoll(argv[8]) : 0; int reps = argc > 9 ? atoi(argv[9]) : 3;
    if (s < 0 || t < 0 || s >= g.n || t >= g.n) { printf("{\"error\":\"node out of range\"}"); return 1; }
    if (delta <= 0) delta = auto_delta(g);
    vector<ll> dist; ll work = 0; double best = 1e18;
    for (int r = 0; r < max(1, reps); r++) {
        double t0 = omp_get_wtime();
        if (algo == "dijkstra") dijkstra(g, s, t, dist, work);
        else delta_stepping(g, s, t, delta, T, dist, work);
        best = min(best, (omp_get_wtime() - t0) * 1000.0);
    }
    bool found = dist[t] < INF;
    vector<int> path = found ? reconstruct(g, dist, s, t) : vector<int>();
    printf("{\"algo\":\"%s\",\"threads\":%d,\"found\":%s,\"dist\":%lld,\"ms\":%.4f,\"work\":%lld,\"delta\":%lld,\"hops\":%zu,\"path\":",
           algo.c_str(), algo == "dijkstra" ? 1 : T, found ? "true" : "false", found ? dist[t] : -1LL, best, work, delta, path.size());
    jpath(path); printf("}");
    return 0;
}

static int cmd_fleet(const Graph& g, int argc, char** argv) {
    string mode = argv[4]; int T = mode == "seq" ? 1 : atoi(argv[5]), K = atoi(argv[6]); unsigned seed = atoi(argv[7]);
    auto P = random_pairs(g.n, K, seed);
    vector<ll> res; vector<vector<int>> paths;
    double ms = run_batch(g, P, T, res, &paths);
    ll total = 0; for (ll r : res) if (r > 0) total += r;
    printf("{\"mode\":\"%s\",\"threads\":%d,\"vehicles\":%d,\"ms\":%.4f,\"totalDist\":%lld,\"routes\":[", mode.c_str(), T, K, ms, total);
    for (int i = 0; i < K; i++) {
        printf("%s{\"src\":%d,\"dst\":%d,\"dist\":%lld,\"path\":", i ? "," : "", P[i].first, P[i].second, res[i]);
        jpath(paths[i]); printf("}");
    }
    printf("]}"); return 0;
}

static int cmd_bench(const Graph& g, int argc, char** argv) {
    auto sizes = csv(argv[4]); auto threads = csv(argv[5]);
    int reps = atoi(argv[6]), Q = atoi(argv[7]); ll dopt = argc > 8 ? atoll(argv[8]) : 0;
    bool first = true;
    printf("{\"rows\":[");
    auto emit = [&](int n, size_t m, const char* algo, int T, double ms, bool ok) {
        printf("%s{\"nodes\":%d,\"edges\":%zu,\"algo\":\"%s\",\"threads\":%d,\"ms\":%.4f,\"verified\":%s}",
               first ? "" : ",", n, m, algo, T, ms, ok ? "true" : "false");
        first = false; fflush(stdout);
    };
    for (int target : sizes) {
        Graph sub; const Graph* G = &g;
        if (target < g.n) { sub = induced_ball(g, g.center, target); G = &sub; }
        const Graph& H = *G; int s = (G == &g) ? g.center : 0;
        ll delta = dopt > 0 ? dopt : auto_delta(H);
        vector<ll> ref, d2; ll w = 0; double best = 1e18;
        for (int r = 0; r < reps; r++) {
            double t0 = omp_get_wtime(); dijkstra(H, s, -1, ref, w);
            best = min(best, (omp_get_wtime() - t0) * 1000.0);
        }
        emit(H.n, H.to.size(), "dijkstra", 1, best, true);
        for (int T : threads) {
            best = 1e18;
            for (int r = 0; r < reps; r++) {
                double t0 = omp_get_wtime(); delta_stepping(H, s, -1, delta, T, d2, w);
                best = min(best, (omp_get_wtime() - t0) * 1000.0);
            }
            emit(H.n, H.to.size(), "delta-omp", T, best, d2 == ref);
        }
        if (Q > 0) {
            auto P = random_pairs(H.n, Q, 42); vector<ll> base, cur;
            for (int T : threads) {
                best = 1e18;
                for (int r = 0; r < reps; r++) best = min(best, run_batch(H, P, T, cur));
                if (base.empty()) base = cur;
                emit(H.n, H.to.size(), "batch-omp", T, best, cur == base);
            }
        }
    }
    printf("]}"); return 0;
}

static int cmd_verify(const Graph& g, int argc, char** argv) {
    int Q = atoi(argv[4]); auto P = random_pairs(g.n, Q, 7); int bad = 0;
    ll delta = auto_delta(g);
    for (auto& pr : P) {
        vector<ll> a, b; ll w;
        dijkstra(g, pr.first, -1, a, w);
        for (int T : {1, 2, 4}) { delta_stepping(g, pr.first, -1, delta, T, b, w); if (a != b) bad++; }
        delta_stepping(g, pr.first, pr.second, delta, 4, b, w);       // early-exit variant
        if (b[pr.second] != a[pr.second]) bad++;
    }
    printf("{\"queries\":%d,\"mismatches\":%d,\"passed\":%s}", Q, bad, bad ? "false" : "true");
    return bad != 0;
}

int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage: router <gr> <co> <route|fleet|bench|verify> ...\n"); return 2; }
    Graph g;
    if (!load_graph(argv[1], argv[2], g)) { printf("{\"error\":\"cannot load graph\"}"); return 2; }
    string c = argv[3];
    if (c == "route" && argc >= 8) return cmd_route(g, argc, argv);
    if (c == "fleet" && argc >= 8) return cmd_fleet(g, argc, argv);
    if (c == "bench" && argc >= 8) return cmd_bench(g, argc, argv);
    if (c == "verify") return cmd_verify(g, argc, argv);
    printf("{\"error\":\"bad command\"}"); return 2;
}

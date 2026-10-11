# Parallel Route Planner – efficient vehicle routing on a large road network

Dataset: DIMACS 9th Challenge `USA-road-d.BAY` (San Francisco Bay Area, 321,270 intersections, 800,172 directed road segments, weights = distance).

## Run it
Requirements: `g++` with OpenMP (Linux, macOS `brew install libomp`, Windows MinGW-w64/MSYS2) and Python 3.8+. No pip packages.
```
python3 server.py          # decompresses data, builds ./router, serves the UI
# open http://localhost:8000
```
Manual build / CLI use: `make`, then e.g.
```
./router data/USA-road-d_BAY_gr data/USA-road-d_BAY_co route delta 4 100 200000
./router data/USA-road-d_BAY_gr data/USA-road-d_BAY_co bench 10000,50000,321270 1,2,4,8 3 32
./router data/USA-road-d_BAY_gr data/USA-road-d_BAY_co verify 20
```

## UI features
* **Route** – click two points on the map (or 🎲 Random), choose *Sequential Dijkstra* or *OpenMP Delta-stepping* and the number of threads; shows path, km, time, work. "Compare all" overlays both routes and reports speedup/efficiency and whether the distances match.
* **Fleet** – dispatch up to 300 vehicles (random origin/destination pairs), sequential vs OpenMP, drawn on the map.
* **Benchmark** – choose network sizes (5k … full 321k nodes), thread counts, repetitions; produces time-vs-size, speedup, efficiency charts, a results table with a correctness flag per run, and CSV download.
* **Correctness check** button (OpenMP vs sequential on random sources).

## Design
| Part | File | Idea |
|---|---|---|
| Sequential baseline | `src/sequential.h` | Dijkstra, binary heap, early exit at target |
| Parallel (inside a query) | `src/parallel.h` `delta_stepping` | Δ-stepping: buckets of width Δ, light edges relaxed repeatedly, heavy edges once; each round is an `omp parallel for` with lock-free `atomic_min` (CAS) on distances, thread-local request lists merged serially |
| Parallel (across queries) | `src/parallel.h` `run_batch` | Each vehicle's query is independent → `omp parallel for schedule(dynamic,1)` |
| Network-size study | `src/graph.h` `induced_ball` | BFS ball of N nodes around the map centre gives sub-networks of any size |
| Path reconstruction | `reconstruct()` | walks the reverse graph with `dist[p]+w==dist[u]`, so no racy parent array is needed |

**Why OpenMP (justification for the report):** shared-memory graph (≈ 30 MB CSR) fits in one node; fine-grained shared distance array with atomic updates is natural in OpenMP; MPI would need graph partitioning with high communication for little gain; CUDA suits huge frontiers but road networks have small, irregular frontiers.

**Expected behaviour (discuss in report):** road networks have low degree and high diameter, so Δ-stepping frontiers are small and a single query is bottlenecked by synchronisation (barriers per bucket) and extra re-relaxations (see `work` column) – speed-ups for one query are modest, and may be <1 on small graphs. The fleet batch is embarrassingly parallel and should scale near-linearly until memory bandwidth saturates. Tune Δ in the Route tab (default 2×average edge weight).

## Verification
`verify` and every benchmark row compare OpenMP distances to sequential Dijkstra (all nodes, 1/2/4 threads, early-exit variant). Tested: 0 mismatches.

## Files to submit (see `docs/`)
`docs/REPORT_TEMPLATE.md` (report outline), `docs/LLM_USAGE_LOG.md` (fill with YOUR real prompts), source in `src/`, data in `data/`, results from the Benchmark tab (CSV + screenshots).

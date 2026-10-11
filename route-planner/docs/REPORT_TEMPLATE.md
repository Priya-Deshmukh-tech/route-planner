# Report outline
1. Problem definition (shortest routes for vehicles on road network; SSSP, point-to-point, fleet)
2. Dataset (DIMACS BAY: n, m, weights, coordinates)
3. Sequential algorithm (Dijkstra, complexity O(m log n))
4. Parallel design: Δ-stepping (OpenMP) and fleet batch; why OpenMP; race conditions & atomics; Δ selection
5. Experimental setup (CPU model, cores, compiler flags, repetitions = best of N)
6. Results: time vs size, speedup, efficiency, threads effect (use Benchmark tab + CSV)
7. Bottleneck analysis (small frontiers, barriers, re-relaxation work, memory bandwidth, load imbalance)
8. Verification (verify command, per-row ✔)
9. LLM usage + critical evaluation
10. Team contributions
11. Conclusion / future work (bidirectional search, contraction hierarchies, MPI partitioning, CUDA)

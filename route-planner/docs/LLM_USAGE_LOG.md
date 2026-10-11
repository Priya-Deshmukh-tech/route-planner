# LLM Usage Log  (fill in with your REAL prompts – do not invent them afterwards)
| Sr | Purpose | Prompt used | How response was used | Modification / Verification |
|----|---------|-------------|-----------------------|-----------------------------|
| 1 | Algorithm understanding | (paste) | | |
| 2 | Parallelization | (paste) | | |
| 3 | Debugging | (paste) | | |
| 4 | Optimization | (paste) | | |

## Critical evaluation of LLM output (required: at least one)
Candidates you can honestly check and write about from this project (verify each yourself before claiming it):
* A naive parallel Dijkstra (parallelising the heap / `omp for` over the priority queue) is not viable – inherently sequential; Δ-stepping was used instead.
* Storing a `parent[]` array updated with the distance in parallel is racy (distance and parent updated by two separate atomics can disagree) – replaced by reconstruction through the reverse graph.
* Measure whether the default Δ suggested (2×avg weight) is best – try several values and report.

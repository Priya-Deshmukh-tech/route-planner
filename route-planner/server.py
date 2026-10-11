#!/usr/bin/env python3
"""Tiny web server (standard library only) for the Route Planner UI.
Run:  python3 server.py   then open http://localhost:8000
It decompresses the dataset, builds the C++ engine if needed and calls it per request."""
import gzip, json, os, shutil, struct, subprocess, sys, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(ROOT, "data")
GR, CO = [os.path.join(DATA, f"USA-road-d_BAY_{k}") for k in ("gr", "co")]
EXE = os.path.join(ROOT, "router.exe" if os.name == "nt" else "router")
LOCK = threading.Lock()          # one experiment at a time -> clean timings


def prepare():
    for p in (GR, CO):
        if not os.path.exists(p):
            print("decompressing", p + ".gz")
            with gzip.open(p + ".gz", "rb") as a, open(p, "wb") as b:
                shutil.copyfileobj(a, b)
    if not os.path.exists(EXE):
        print("building engine ...")
        subprocess.check_call(["g++", "-O3", "-std=c++17", "-fopenmp",
                               os.path.join(ROOT, "src", "main.cpp"), "-o", EXE])
    nb = os.path.join(DATA, "nodes.bin")
    n = 0
    if not os.path.exists(nb):
        pts = []
        with open(CO) as f:
            for line in f:
                if line[0] == "v":
                    _, _, x, y = line.split(); pts += [int(x), int(y)]
        with open(nb, "wb") as f:
            f.write(struct.pack(f"<{len(pts)}i", *pts))


def engine(*args, timeout=900):
    with LOCK:
        r = subprocess.run([EXE, GR, CO, *map(str, args)], capture_output=True, text=True, timeout=timeout)
    if not r.stdout.strip():
        raise RuntimeError(r.stderr or "engine produced no output")
    return json.loads(r.stdout)


def num(d, k, default, lo, hi):
    return max(lo, min(hi, int(d.get(k, default))))


class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def send(self, code, body, ctype="application/json"):
        if not isinstance(body, bytes): body = json.dumps(body).encode()
        self.send_response(code); self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            return self.send(200, open(os.path.join(ROOT, "web", "index.html"), "rb").read(), "text/html")
        if self.path == "/api/nodes.bin":
            return self.send(200, open(os.path.join(DATA, "nodes.bin"), "rb").read(), "application/octet-stream")
        if self.path == "/api/meta":
            return self.send(200, {"cpus": os.cpu_count() or 1,
                                   "nodes": os.path.getsize(os.path.join(DATA, "nodes.bin")) // 8})
        self.send(404, {"error": "not found"})

    def do_POST(self):
        try:
            d = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            cpus = (os.cpu_count() or 1) * 4
            if self.path == "/api/route":
                algo = "delta" if d.get("algo") == "delta" else "dijkstra"
                out = engine("route", algo, num(d, "threads", 1, 1, cpus), num(d, "src", 0, 0, 10**9),
                             num(d, "dst", 1, 0, 10**9), num(d, "delta", 0, 0, 10**9), num(d, "reps", 3, 1, 20))
            elif self.path == "/api/fleet":
                mode = "omp" if d.get("mode") == "omp" else "seq"
                out = engine("fleet", mode, num(d, "threads", 1, 1, cpus), num(d, "k", 20, 1, 300), num(d, "seed", 1, 0, 10**6))
            elif self.path == "/api/bench":
                sizes = ",".join(str(int(s)) for s in d.get("sizes", [10000]))
                threads = ",".join(str(max(1, min(cpus, int(t)))) for t in d.get("threads", [1]))
                out = engine("bench", sizes, threads, num(d, "reps", 3, 1, 10), num(d, "batch", 0, 0, 500))
            elif self.path == "/api/verify":
                out = engine("verify", num(d, "q", 10, 1, 100))
            else:
                return self.send(404, {"error": "not found"})
            self.send(200, out)
        except Exception as e:
            self.send(500, {"error": str(e)})


if __name__ == "__main__":
    prepare()
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    print(f"Open http://localhost:{port}")
    ThreadingHTTPServer(("127.0.0.1", port), H).serve_forever()

CXX ?= g++
CXXFLAGS = -O3 -std=c++17 -fopenmp -Wall
router: src/main.cpp src/graph.h src/sequential.h src/parallel.h
	$(CXX) $(CXXFLAGS) src/main.cpp -o router
clean:
	rm -f router router.exe

#ifndef SOLUTION_H
#define SOLUTION_H

#include <string>
#include "Graph.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

using namespace std;

class Solution{
    public:
        void read_benchmark(Graph &graph, string benchmark_name);

        vector<int> partition(Graph &graph);
        void write_partition(const vector<int> &assignment, string output_name);
        string default_output_path(string benchmark_name);

    private:
        int cut_delta_if_moved(Graph &graph, const vector<int> &assignment, int node_index);
};

#endif

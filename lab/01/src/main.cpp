#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <filesystem>
#include "Net.h"
#include "Node.h"
#include "Graph.h"
#include "evaluate.h"
#include "solution.h"

using namespace std;

int main(int argc, char **argv) {

    Solution solution;

    if(argc != 2) {
        cout << "Usage: ./build/main benchmark_file" << endl;
        exit(-1);
    }
    string benchmark_name = argv[1];
    Graph graph;

    solution.read_benchmark(graph, benchmark_name);    

    cout << "Num nodes: " << graph.get_node_num() << endl;
    cout << "Num nets: " << graph.get_net_num() << endl;

    filesystem::create_directories("build/output");
    string output_name = solution.default_output_path(benchmark_name);
    vector<int> assignment = solution.partition(graph);
    solution.write_partition(assignment, output_name);

    int cut = evaluate(graph, output_name);
    cout << "Partition file: " << output_name << endl;
    cout << "Cut: " << cut << endl;

    return cut < 0 ? -1 : 0;
}

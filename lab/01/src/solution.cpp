#include "solution.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace {
std::string basename_without_extension(const std::string &path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const std::size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

int net_cut_state(Net *net, const std::vector<int> &assignment) {
    bool has_zero = false;
    bool has_one = false;
    for (auto *node : net->get_nodes()) {
        const int index = node->get_index();
        if (index >= static_cast<int>(assignment.size())) {
            continue;
        }
        if (assignment[index] == 0) {
            has_zero = true;
        } else {
            has_one = true;
        }
        if (has_zero && has_one) {
            return 1;
        }
    }
    return 0;
}
}

void Solution::read_benchmark(Graph &graph, string benchmark_name) {
    ifstream file(benchmark_name);

    if(!file.is_open()) {
        cerr << "Failed to open the file!" << endl;
        exit(-1);
    }

    int edge_num, node_num;
    string line;
    getline(file >> ws, line);
    istringstream iss(line);
    iss >> edge_num;
    iss >> node_num;

    
    for(int i = 0; i < edge_num; i++) {
        if (!getline(file, line)) {
            throw std::runtime_error("Unexpected end of benchmark file.");
        }
        istringstream iss(line);
        int node_id;
        
        Net *net = graph.add_net(i);

        while(iss >> node_id) {
            Node *node = graph.get_or_create_node(node_id);
            node->add_net(net);
            net->add_node(node);
        }
        
    }

    for (int node_id = 1; node_id <= node_num; ++node_id) {
        graph.get_or_create_node(node_id);
    }

    file.close();
}

vector<int> Solution::partition(Graph &graph) {
    const int node_num = graph.get_node_num();
    vector<int> assignment(node_num + 1, 1);
    const int target_zero = node_num / 2;
    const int min_zero = static_cast<int>(node_num * 0.48);
    const int max_zero = static_cast<int>(node_num * 0.52);

    vector<Node *> nodes = graph.get_nodes();
    sort(nodes.begin(), nodes.end(), [](Node *lhs, Node *rhs) {
        if (lhs->get_nets().size() != rhs->get_nets().size()) {
            return lhs->get_nets().size() > rhs->get_nets().size();
        }
        return lhs->get_index() < rhs->get_index();
    });

    int zero_count = 0;
    for (auto *node : nodes) {
        if (zero_count < target_zero) {
            assignment[node->get_index()] = 0;
            ++zero_count;
        }
    }

    constexpr int kMaxPasses = 5;
    for (int pass = 0; pass < kMaxPasses; ++pass) {
        bool improved = false;
        for (auto *node : nodes) {
            const int index = node->get_index();
            const int next_zero_count = zero_count + (assignment[index] == 0 ? -1 : 1);
            if (next_zero_count < min_zero || next_zero_count > max_zero) {
                continue;
            }
            if (cut_delta_if_moved(graph, assignment, index) < 0) {
                assignment[index] = 1 - assignment[index];
                zero_count = next_zero_count;
                improved = true;
            }
        }
        if (!improved) {
            break;
        }
    }

    return assignment;
}

int Solution::cut_delta_if_moved(Graph &graph, const vector<int> &assignment, int node_index) {
    Node *node = graph.get_node(node_index);
    if (node == nullptr) {
        return 0;
    }

    int before = 0;
    int after = 0;
    vector<int> moved = assignment;
    moved[node_index] = 1 - moved[node_index];

    for (auto *net : node->get_nets()) {
        before += net_cut_state(net, assignment);
        after += net_cut_state(net, moved);
    }

    return after - before;
}

void Solution::write_partition(const vector<int> &assignment, string output_name) {
    ofstream output(output_name);
    if (!output.is_open()) {
        cerr << "Failed to open output file: " << output_name << endl;
        exit(-1);
    }

    for (size_t node_index = 1; node_index < assignment.size(); ++node_index) {
        output << assignment[node_index] << '\n';
    }
}

string Solution::default_output_path(string benchmark_name) {
    return "build/output/" + basename_without_extension(benchmark_name) + "_partition.txt";
}

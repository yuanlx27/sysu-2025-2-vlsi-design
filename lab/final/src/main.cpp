#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string benchmark_path;
    int parts = 4;
    double min_ratio = 0.0;
    int passes = 8;
};

struct Hypergraph {
    int node_count = 0;
    std::vector<std::vector<int>> nets;
    std::vector<std::vector<int>> incident_nets;
};

struct Metrics {
    int cut = 0;
    int min_part_size = 0;
    int max_part_size = 0;
    int imbalance = 0;
};

std::string basename_without_extension(const std::string &path) {
    const std::filesystem::path fs_path(path);
    return fs_path.stem().string();
}

void print_usage(const char *program) {
    std::cerr << "Usage: " << program
              << " <benchmark.hgr> [--parts K] [--min-r R] [--passes N]\n";
}

int parse_int(const std::string &value, const std::string &name) {
    std::size_t parsed = 0;
    int result = 0;
    try {
        result = std::stoi(value, &parsed);
    } catch (const std::exception &) {
        throw std::runtime_error("Invalid integer for " + name + ": " + value);
    }
    if (parsed != value.size()) {
        throw std::runtime_error("Invalid integer for " + name + ": " + value);
    }
    return result;
}

double parse_double(const std::string &value, const std::string &name) {
    std::size_t parsed = 0;
    double result = 0.0;
    try {
        result = std::stod(value, &parsed);
    } catch (const std::exception &) {
        throw std::runtime_error("Invalid number for " + name + ": " + value);
    }
    if (parsed != value.size()) {
        throw std::runtime_error("Invalid number for " + name + ": " + value);
    }
    return result;
}

Options parse_options(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        throw std::runtime_error("Missing benchmark file.");
    }

    Options options;
    options.benchmark_path = argv[1];

    for (int i = 2; i < argc; ++i) {
        const std::string flag = argv[i];
        if (i + 1 >= argc) {
            throw std::runtime_error("Missing value for option: " + flag);
        }
        const std::string value = argv[++i];
        if (flag == "--parts") {
            options.parts = parse_int(value, flag);
        } else if (flag == "--min-r") {
            options.min_ratio = parse_double(value, flag);
        } else if (flag == "--passes") {
            options.passes = parse_int(value, flag);
        } else {
            throw std::runtime_error("Unknown option: " + flag);
        }
    }

    if (options.parts < 2) {
        throw std::runtime_error("--parts must be at least 2.");
    }
    if (options.passes < 0) {
        throw std::runtime_error("--passes must be non-negative.");
    }
    const double max_ratio = 1.0 / static_cast<double>(options.parts);
    if (options.min_ratio < 0.0 || options.min_ratio > max_ratio + 1e-12) {
        std::ostringstream message;
        message << "--min-r must be in [0, " << max_ratio << "] for "
                << options.parts << " parts.";
        throw std::runtime_error(message.str());
    }

    return options;
}

Hypergraph read_hgr(const std::string &path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open benchmark file: " + path);
    }

    int edge_count = 0;
    int node_count = 0;
    if (!(input >> edge_count >> node_count)) {
        throw std::runtime_error("Failed to read benchmark header.");
    }
    if (edge_count < 0 || node_count < 0) {
        throw std::runtime_error("Benchmark header contains negative counts.");
    }

    std::string line;
    std::getline(input, line);

    Hypergraph graph;
    graph.node_count = node_count;
    graph.nets.reserve(edge_count);
    graph.incident_nets.assign(node_count, {});

    for (int net_id = 0; net_id < edge_count; ++net_id) {
        if (!std::getline(input, line)) {
            throw std::runtime_error("Unexpected end of file while reading nets.");
        }

        std::istringstream iss(line);
        int one_based_node = 0;
        std::vector<int> net;
        while (iss >> one_based_node) {
            if (one_based_node < 1 || one_based_node > node_count) {
                std::ostringstream message;
                message << "Node id " << one_based_node
                        << " is outside the valid range [1, " << node_count << "].";
                throw std::runtime_error(message.str());
            }
            const int node = one_based_node - 1;
            net.push_back(node);
            graph.incident_nets[node].push_back(net_id);
        }
        graph.nets.push_back(std::move(net));
    }

    return graph;
}

std::vector<int> balanced_capacities(int node_count, int parts) {
    std::vector<int> capacities(parts, node_count / parts);
    for (int part = 0; part < node_count % parts; ++part) {
        ++capacities[part];
    }
    return capacities;
}

int lower_bound_size(int node_count, double min_ratio) {
    return static_cast<int>(std::floor(min_ratio * static_cast<double>(node_count) + 1e-12));
}

int net_cut_state(const std::vector<int> &net, const std::vector<int> &assignment) {
    if (net.empty()) {
        return 0;
    }
    const int first_part = assignment[net.front()];
    for (int node : net) {
        if (assignment[node] != first_part) {
            return 1;
        }
    }
    return 0;
}

int total_cut(const Hypergraph &graph, const std::vector<int> &assignment) {
    int cut = 0;
    for (const auto &net : graph.nets) {
        cut += net_cut_state(net, assignment);
    }
    return cut;
}

int delta_if_assigned(const Hypergraph &graph,
                      const std::vector<int> &assignment,
                      int node,
                      int next_part) {
    int before = 0;
    int after = 0;

    for (int net_id : graph.incident_nets[node]) {
        const auto &net = graph.nets[net_id];
        before += net_cut_state(net, assignment);

        int first_part = -1;
        bool is_cut = false;
        for (int net_node : net) {
            const int part = (net_node == node) ? next_part : assignment[net_node];
            if (first_part < 0) {
                first_part = part;
            } else if (part != first_part) {
                is_cut = true;
                break;
            }
        }
        after += is_cut ? 1 : 0;
    }

    return after - before;
}

int added_cut_for_initial_assignment(const Hypergraph &graph,
                                     const std::vector<int> &assignment,
                                     int node,
                                     int part) {
    int added = 0;
    for (int net_id : graph.incident_nets[node]) {
        int before_part = -1;
        bool before_cut = false;
        int after_part = part;
        bool after_cut = false;

        for (int net_node : graph.nets[net_id]) {
            if (net_node == node || assignment[net_node] < 0) {
                continue;
            }
            const int assigned_part = assignment[net_node];
            if (before_part < 0) {
                before_part = assigned_part;
            } else if (assigned_part != before_part) {
                before_cut = true;
            }
            if (assigned_part != after_part) {
                after_cut = true;
            }
        }
        if (!before_cut && after_cut) {
            ++added;
        }
    }
    return added;
}

std::vector<int> initial_partition(const Hypergraph &graph, int parts) {
    std::vector<int> assignment(graph.node_count, -1);
    std::vector<int> sizes(parts, 0);
    const std::vector<int> capacities = balanced_capacities(graph.node_count, parts);

    std::vector<int> order(graph.node_count);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        const std::size_t lhs_degree = graph.incident_nets[lhs].size();
        const std::size_t rhs_degree = graph.incident_nets[rhs].size();
        if (lhs_degree != rhs_degree) {
            return lhs_degree > rhs_degree;
        }
        return lhs < rhs;
    });

    for (int node : order) {
        int best_part = -1;
        int best_added_cut = std::numeric_limits<int>::max();
        for (int part = 0; part < parts; ++part) {
            if (sizes[part] >= capacities[part]) {
                continue;
            }
            const int added_cut = added_cut_for_initial_assignment(graph, assignment, node, part);
            if (added_cut < best_added_cut ||
                (added_cut == best_added_cut &&
                 (best_part < 0 || sizes[part] < sizes[best_part] ||
                  (sizes[part] == sizes[best_part] && part < best_part)))) {
                best_added_cut = added_cut;
                best_part = part;
            }
        }

        if (best_part < 0) {
            throw std::runtime_error("Internal error: no available part during initialization.");
        }
        assignment[node] = best_part;
        ++sizes[best_part];
    }

    return assignment;
}

std::vector<int> improve_partition(const Hypergraph &graph,
                                   std::vector<int> assignment,
                                   int parts,
                                   int lower,
                                   int passes) {
    std::vector<int> sizes(parts, 0);
    for (int part : assignment) {
        ++sizes[part];
    }
    const std::vector<int> capacities = balanced_capacities(graph.node_count, parts);

    for (int pass = 0; pass < passes; ++pass) {
        bool improved = false;
        for (int node = 0; node < graph.node_count; ++node) {
            const int old_part = assignment[node];
            if (sizes[old_part] - 1 < lower) {
                continue;
            }

            int best_part = old_part;
            int best_delta = 0;
            for (int part = 0; part < parts; ++part) {
                if (part == old_part || sizes[part] + 1 > capacities[part]) {
                    continue;
                }
                const int delta = delta_if_assigned(graph, assignment, node, part);
                if (delta < best_delta ||
                    (delta == best_delta && delta < 0 &&
                     (sizes[part] < sizes[best_part] ||
                      (sizes[part] == sizes[best_part] && part < best_part)))) {
                    best_delta = delta;
                    best_part = part;
                }
            }

            if (best_part != old_part) {
                assignment[node] = best_part;
                --sizes[old_part];
                ++sizes[best_part];
                improved = true;
            }
        }
        if (!improved) {
            break;
        }
    }

    return assignment;
}

Metrics evaluate_partition(const Hypergraph &graph, const std::vector<int> &assignment, int parts) {
    std::vector<int> sizes(parts, 0);
    for (int part : assignment) {
        ++sizes[part];
    }

    Metrics metrics;
    metrics.cut = total_cut(graph, assignment);
    metrics.min_part_size = *std::min_element(sizes.begin(), sizes.end());
    metrics.max_part_size = *std::max_element(sizes.begin(), sizes.end());
    metrics.imbalance = metrics.max_part_size - metrics.min_part_size;
    return metrics;
}

void write_partition(const std::vector<int> &assignment, const std::string &output_path) {
    std::filesystem::create_directories(std::filesystem::path(output_path).parent_path());
    std::ofstream output(output_path);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open output file: " + output_path);
    }
    for (int part : assignment) {
        output << part << '\n';
    }
}

std::string output_path_for(const std::string &benchmark_path, int parts) {
    return "build/output/" + basename_without_extension(benchmark_path) + "_k" +
           std::to_string(parts) + "_partition.txt";
}

} // namespace

int main(int argc, char **argv) {
    try {
        const Options options = parse_options(argc, argv);
        const auto start_time = std::chrono::steady_clock::now();

        const Hypergraph graph = read_hgr(options.benchmark_path);
        const int lower = lower_bound_size(graph.node_count, options.min_ratio);

        std::vector<int> assignment = initial_partition(graph, options.parts);
        assignment = improve_partition(graph, assignment, options.parts, lower, options.passes);

        const Metrics metrics = evaluate_partition(graph, assignment, options.parts);
        const std::string output_path = output_path_for(options.benchmark_path, options.parts);
        write_partition(assignment, output_path);

        const auto end_time = std::chrono::steady_clock::now();
        const double runtime_seconds =
            std::chrono::duration<double>(end_time - start_time).count();

        std::cout << "Num nodes: " << graph.node_count << '\n';
        std::cout << "Num nets: " << graph.nets.size() << '\n';
        std::cout << "Parts: " << options.parts << '\n';
        std::cout << "Min ratio: " << options.min_ratio << '\n';
        std::cout << "Cut: " << metrics.cut << '\n';
        std::cout << "Imbalance: " << metrics.imbalance << '\n';
        std::cout << "Min part size: " << metrics.min_part_size << '\n';
        std::cout << "Max part size: " << metrics.max_part_size << '\n';
        std::cout << "Partition file: " << output_path << '\n';
        std::cout << std::fixed << std::setprecision(6)
                  << "Runtime seconds: " << runtime_seconds << '\n';
    } catch (const std::exception &error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}

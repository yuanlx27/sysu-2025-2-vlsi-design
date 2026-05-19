#import "@local/bubble-sysu:0.1.0": *

#show: report.with(
  title: "实验一：图划分算法",
  subtitle: "VLSI 设计导论小作业",
  student: (name: "元朗曦", id: "23336294"),
  school: "计算机学院",
  major: "计算机科学与技术",
  class: "计八",
)

= 实验目的

本实验要求设计并实现一个图划分算法。给定一个由顶点集合 $V$ 和超边集合构成的电路网表，需要输出两个划分 $V_1$ 和 $V_2$，并使 $V_1$ 与 $V_2$ 之间的割代价尽可能小。这里的割代价指跨越两个划分的连接数量：如果一条超边同时连接到两个划分中的顶点，则该超边会贡献割代价。

在优化割代价的同时，划分结果必须满足规模平衡约束。对于任意一个划分 $V_i$，都要求：

$
  1 / 2 - epsilon <= abs(V_i) / abs(V) <= 1 / 2 + epsilon
$

其中 $epsilon = 0.02$，$|V|$ 表示图中顶点总数。因此，本实验不是单纯追求最小割，而是在“割代价尽可能小”和“两个划分规模近似相等”之间进行约束优化。

根据实验文档要求，小作业阶段需要实现算法，但暂时不提交代码，代码将在大作业阶段统一提交。本次报告需要说明算法逻辑和实现思路，展示算法运行结果表格，报告每个数据集的割代价，并将结果与给出的参考最优解进行比较。同时，报告还需要总结当前方法的优点、缺点和下一步改进方向。

因此，本实验的具体目标可以概括为：在助教提供的初始代码框架上完成超图二划分算法，实现合法的划分结果输出，使用评估函数统计割代价，并为后续在完整 benchmark 上对比最优解和继续优化算法打下基础。

= 实验原理

== 超图划分模型

电路网表可以抽象为超图 $H = (V, E)$。其中，$V$ 表示电路中的模块或单元，$E$ 表示连接多个模块的线网。与普通图中的边只连接两个顶点不同，超边可以同时连接多个顶点，因此更适合表示真实电路网表中的一条 net。

二划分问题要求将顶点集合 $V$ 分为两个不相交集合 $X$ 和 $Y$，并满足：

$ X union Y = V, quad X inter Y = emptyset $

课程要求两个划分的规模满足：

$
  0.48 <= abs(X) / abs(V) <= 0.52
$

$
  0.48 <= abs(Y) / abs(V) <= 0.52
$

割代价定义为跨越两个划分的超边数量。对于某一条超边 $e$，如果它连接的顶点中既有属于 $X$ 的顶点，也有属于 $Y$ 的顶点，则该超边贡献 1 的割代价。总割代价可以写为：

$ "cut"(X, Y) = sum_(e in E) "I"(e inter X != emptyset and e inter Y != emptyset) $

因此，算法目标是在满足规模平衡约束的前提下，使 $"cut"(X, Y)$ 尽可能小。

== 实现思路

本实验采用确定性启发式算法。算法分为两个阶段：初始划分和局部改进。

初始划分阶段先统计每个顶点连接的超边数量，即顶点度数。直观上，度数较高的顶点出现在更多线网中，对割代价影响更大。因此程序先按照度数从高到低排序；如果度数相同，则按照顶点编号从小到大排序，以保证算法在同一输入上具有稳定输出。排序后，前一半顶点放入划分 0，其余顶点放入划分 1，由此得到一个严格接近二等分的初始解。

局部改进阶段尝试移动单个顶点。对每个顶点，算法计算将其移动到另一个划分后，与该顶点相关的超边割状态变化。如果移动能降低割代价，并且移动后仍满足 $1 / 2 +- 0.02$ 的平衡约束，则接受这次移动。程序最多进行 5 轮扫描，当某一轮没有任何顶点被移动时提前结束。

这种方法的特点是实现简单、输出稳定、便于解释，适合作为小作业阶段的 baseline。它并不是全局最优算法，可能陷入局部最优；但它能完整体现超图读取、平衡约束检查、割代价评估和结果输出等实验核心流程。

= 实验内容

== 项目结构

本实验代码放在 `lab/01/src` 目录下，构建入口放在 `lab/01/Makefile`。编译产生的目标文件、可执行文件和划分输出统一放入 `lab/01/build`，便于通过 `.gitignore` 忽略生成产物。

主要文件职责如下：

- `Graph.cpp/.h`：保存图中的顶点列表、超边列表，以及编号到对象指针的映射。
- `Node.cpp/.h`：保存顶点编号和该顶点连接的超边。
- `Net.cpp/.h`：保存超边编号和该超边连接的顶点。
- `solution.cpp/.h`：实现 benchmark 读取、划分算法、结果输出路径和文件写入。
- `evaluate.cpp/.h`：根据输出文件重新构造两个划分，并计算割代价。
- `main.cpp`：程序入口，负责串联读入、划分、输出和评估。

== 输入读取

读取函数位于 `Solution::read_benchmark`。程序先读取第一行的超边数量和顶点数量，然后逐行读取每条超边连接的顶点。对于每个顶点编号，程序通过 `Graph::get_or_create_node` 获取或创建对应的 `Node` 对象，并同时维护 `Node` 到 `Net`、`Net` 到 `Node` 的双向关系。

```cpp
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
```

由于 `.hgr` 文件中可能存在没有出现在任何超边中的孤立顶点，读取完超边后，程序额外遍历 `1` 到 `node_num`，保证所有顶点编号都有对应的 `Node` 对象。

```cpp
for (int node_id = 1; node_id <= node_num; ++node_id) {
    graph.get_or_create_node(node_id);
}
```

== 初始划分

初始划分使用顶点度数作为启发式信息。程序复制一份顶点列表，并按照连接超边数量从高到低排序。排序后的前一半顶点放入划分 0，其余顶点保持在默认划分 1。

```cpp
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
```

这里的 `assignment` 是一个以顶点编号为下标的数组，`assignment[i] = 0` 表示顶点 `i` 属于第一个划分，`assignment[i] = 1` 表示属于第二个划分。采用顶点编号作为下标可以让输出文件顺序自然对应课程要求中的“每行表示一个顶点的划分类别”。

== 局部改进

局部改进函数 `cut_delta_if_moved` 用于判断移动某个顶点是否能降低割代价。函数只检查与该顶点相连的超边，而不是每次重新计算整张图的割代价。这样可以降低一次移动评估的计算量。

```cpp
int Solution::cut_delta_if_moved(Graph &graph,
                                 const vector<int> &assignment,
                                 int node_index) {
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
```

主循环中，程序先计算移动后划分 0 的顶点数量。如果移动会违反 $48%$ 到 $52%$ 的规模约束，则直接跳过。如果移动后割代价下降，即 `cut_delta_if_moved` 返回负数，则接受该移动。

```cpp
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
```

== 输出与评估

程序默认将结果输出到 `build/output` 目录。输出文件名根据 benchmark 名称自动生成，例如输入 `data/ibm01.hgr` 时，输出文件为 `build/output/ibm01_partition.txt`。

```cpp
filesystem::create_directories("build/output");
string output_name = solution.default_output_path(benchmark_name);
vector<int> assignment = solution.partition(graph);
solution.write_partition(assignment, output_name);

int cut = evaluate(graph, output_name);
cout << "Partition file: " << output_name << endl;
cout << "Cut: " << cut << endl;
```

输出文件每一行是一个顶点的划分类别。第 1 行对应顶点 1，第 2 行对应顶点 2，依此类推。

= 实验结果

== 运行方式

在 `lab/01` 目录下执行脚本即可完成编译、批量运行测试并生成报告所需结果表：

```bash
bash run.sh
```

== ISPD Benchmark 测试结果

本次实验使用课程发布的 `ISPD_benchmark.zip` 数据集进行测试。数据集解压后放置在 `lab/01/data` 目录下，共包含 `ibm01.hgr` 到 `ibm18.hgr` 18 个测试样例。运行 `bash run.sh` 后，脚本会依次运行所有 `.hgr` 文件，并将汇总结果写入 `report/result.csv`。

表中的“参考最优割代价”取自实验文档中 $epsilon = 2%$ 一列。“相对差距”按如下公式计算：

$ ("本算法割代价" - "参考最优割代价") / "参考最优割代价" times 100% $

#let result-data = csv("./result.csv")
#let result-header = result-data.first().map(it => strong(text(size: 9pt, it)))
#let result-rows = result-data.slice(1).flatten()

#figure(
  table(
    columns: 7,
    align: center,
    inset: 2pt,
    table.header(..result-header),
    ..result-rows,
  ),
  caption: [ISPD benchmark 测试结果],
)

== 结果分析

从实际测试结果可以看出，程序已经完成了以下功能：

1. 能正确读取 `.hgr` 格式输入，并构建顶点与超边之间的双向关系。
2. 能生成与顶点数量一致的划分输出文件。
3. 能调用 `evaluate` 函数检查平衡约束并计算割代价。
4. 能通过 `run.sh` 批量运行 18 个 ISPD benchmark，并将结果写入 `report/result.csv`。

从割代价来看，当前算法与参考最优解存在明显差距。原因在于本算法只是一个确定性的启发式 baseline：初始划分主要依据顶点度数，无法捕捉超图的全局连接结构；局部改进只考虑单个顶点移动，没有使用更强的增益维护、锁定机制或成对交换策略，因此容易在初始解附近陷入局部最优。

从运行时间来看，小规模样例可以在数秒内完成，大规模样例耗时明显增加。其中 `ibm16` 用时最长，达到 224 秒。除了样例规模更大以外，一个重要原因是 `Graph::get_or_create_node` 沿用了官方初始代码中的线性查找方式，在读取大规模 benchmark 时会带来额外开销。后续如果需要提升效率，可以优先将节点查找改为直接使用 `node_map`，并进一步优化局部移动时的割代价增量维护。

= 实验总结

本实验完成了超图二划分算法的基本实现。程序在助教提供的初始框架上补充了读取后孤立顶点处理、基于顶点度数的初始划分、满足平衡约束的单点局部改进、结果文件输出和割代价评估。通过 ISPD benchmark 测试，程序能够完整跑通从输入读取到输出评估的流程，并生成可用于报告分析的结果表格。

当前方法更侧重于可解释性和稳定性，适合作为 baseline。若继续改进，可以从三个方向展开：第一，引入更强的局部搜索策略，例如 FM 算法中的增益桶和成对交换；第二，在保持固定随机种子的前提下加入多次扰动初始解，提高跳出局部最优的能力；第三，优化图数据结构和增量割代价维护方式，提高在 IBM 系列大规模 benchmark 上的运行效率。
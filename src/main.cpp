#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

using namespace std;

/*
 * 项目名称：
 * 基于图结构与 PageRank 算法的《三国演义》人物关系网络分析
 *
 * 程序功能：
 * 1. 从 data/character_edges_simple.csv 读取人物关系数据；
 * 2. 将人物关系建模为加权无向图；
 * 3. 使用邻接表存储图结构；
 * 4. 统计人物节点数量、关系边数量、普通度数和加权度数；
 * 5. 使用考虑边权重的 PageRank 算法分析人物重要性；
 * 6. 将分析结果输出到 output/degree_result.txt 和 output/pagerank_result.txt。
 */

// 邻接表中的一条边：to 表示邻接点编号，weight 表示关系强度。
struct Edge {
    int to;
    double weight;
};

// PageRank 计算结果，便于输出算法收敛信息。
struct PageRankResult {
    vector<double> score;
    int iterations = 0;
    double finalDiff = 0.0;
};

class Graph {
private:
    vector<string> names;                // 编号 -> 人物姓名
    unordered_map<string, int> nameToId; // 人物姓名 -> 编号
    vector<vector<Edge>> adj;            // 加权无向图的邻接表
    set<pair<int, int>> edgeSet;         // 记录无向边，用于统计唯一边数

    void addNeighbor(int u, int v, double weight) {
        // 如果同一对人物在 CSV 中重复出现，则将关系强度累加。
        for (Edge &edge : adj[u]) {
            if (edge.to == v) {
                edge.weight += weight;
                return;
            }
        }
        adj[u].push_back({v, weight});
    }

public:
    int getOrCreateId(const string &name) {
        auto it = nameToId.find(name);
        if (it != nameToId.end()) {
            return it->second;
        }

        int id = static_cast<int>(names.size());
        names.push_back(name);
        nameToId[name] = id;
        adj.push_back(vector<Edge>());
        return id;
    }

    void addUndirectedEdge(const string &source, const string &target, double weight) {
        int u = getOrCreateId(source);
        int v = getOrCreateId(target);

        // 自环不参与人物关系网络分析。
        if (u == v) {
            return;
        }

        int a = min(u, v);
        int b = max(u, v);
        edgeSet.insert({a, b});

        // 无向图需要在两个方向都加入邻接关系。
        addNeighbor(u, v, weight);
        addNeighbor(v, u, weight);
    }

    int nodeCount() const {
        return static_cast<int>(names.size());
    }

    int edgeCount() const {
        return static_cast<int>(edgeSet.size());
    }

    const vector<string> &getNames() const {
        return names;
    }

    const vector<vector<Edge>> &getAdjacencyList() const {
        return adj;
    }

    // 普通度数：与该人物直接相连的不同人物数量。
    int degree(int u) const {
        return static_cast<int>(adj[u].size());
    }

    // 加权度数：该人物所有邻接边权重之和，表示总关系强度。
    double weightedDegree(int u) const {
        double sum = 0.0;
        for (const Edge &edge : adj[u]) {
            sum += edge.weight;
        }
        return sum;
    }

    vector<double> weightedDegrees() const {
        vector<double> result(nodeCount(), 0.0);
        for (int i = 0; i < nodeCount(); ++i) {
            result[i] = weightedDegree(i);
        }
        return result;
    }
};

string trim(const string &s) {
    size_t start = 0;
    while (start < s.size() && isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }

    size_t end = s.size();
    while (end > start && isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }

    return s.substr(start, end - start);
}

void removeUtf8Bom(string &line) {
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line = line.substr(3);
    }
}

vector<string> splitCsvLine(const string &line) {
    vector<string> cols;
    string item;
    stringstream ss(line);

    while (getline(ss, item, ',')) {
        cols.push_back(trim(item));
    }

    return cols;
}

string toLowerAscii(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return s;
}

bool isHeaderLine(const vector<string> &cols) {
    if (cols.size() < 3) {
        return false;
    }

    return toLowerAscii(cols[0]) == "source" &&
           toLowerAscii(cols[1]) == "target" &&
           toLowerAscii(cols[2]) == "weight";
}

bool createDirectoryIfNeeded(const string &path) {
#ifdef _WIN32
    int result = _mkdir(path.c_str());
#else
    int result = mkdir(path.c_str(), 0755);
#endif

    if (result == 0 || errno == EEXIST) {
        return true;
    }

    cerr << "错误：无法创建输出目录：" << path << "\n";
    return false;
}

bool loadGraphFromCsv(const string &filename, Graph &graph) {
    ifstream fin(filename, ios::binary);

    if (!fin.is_open()) {
        cerr << "错误：无法打开数据文件：" << filename << "\n";
        cerr << "请确认 data/character_edges_simple.csv 是否存在。\n";
        return false;
    }

    string line;
    int lineNumber = 0;
    int validCount = 0;
    int invalidCount = 0;

    while (getline(fin, line)) {
        ++lineNumber;
        if (lineNumber == 1) {
            removeUtf8Bom(line);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        vector<string> cols = splitCsvLine(line);
        if (lineNumber == 1 && isHeaderLine(cols)) {
            continue;
        }

        if (cols.size() != 3) {
            ++invalidCount;
            cerr << "警告：第 " << lineNumber << " 行不是 source,target,weight 三列，已跳过。\n";
            continue;
        }

        string source = trim(cols[0]);
        string target = trim(cols[1]);
        string weightText = trim(cols[2]);

        if (source.empty() || target.empty()) {
            ++invalidCount;
            cerr << "警告：第 " << lineNumber << " 行人物名称为空，已跳过。\n";
            continue;
        }

        double weight = 0.0;
        try {
            size_t parsedLength = 0;
            weight = stod(weightText, &parsedLength);
            if (parsedLength != weightText.size()) {
                throw invalid_argument("extra characters");
            }
        } catch (...) {
            ++invalidCount;
            cerr << "警告：第 " << lineNumber << " 行权重格式错误，已跳过。\n";
            continue;
        }

        if (weight <= 0.0) {
            ++invalidCount;
            cerr << "警告：第 " << lineNumber << " 行权重不是正数，已跳过。\n";
            continue;
        }

        graph.addUndirectedEdge(source, target, weight);
        ++validCount;
    }

    cout << "数据读取完成。\n";
    cout << "有效关系记录数：" << validCount << "\n";
    if (invalidCount > 0) {
        cout << "无效记录数：" << invalidCount << "\n";
    }

    return true;
}

PageRankResult calculateWeightedPageRank(
    const Graph &graph,
    double dampingFactor,
    int maxIterations,
    double epsilon) {

    int n = graph.nodeCount();
    const vector<vector<Edge>> &adj = graph.getAdjacencyList();
    vector<double> weightedDegrees = graph.weightedDegrees();

    vector<double> current(n, 1.0 / n);
    vector<double> next(n, 0.0);

    PageRankResult result;

    for (int iter = 1; iter <= maxIterations; ++iter) {
        fill(next.begin(), next.end(), (1.0 - dampingFactor) / n);

        double danglingMass = 0.0;

        for (int u = 0; u < n; ++u) {
            // 如果某个节点没有邻边，则把它的 PageRank 平均分配给所有节点。
            if (weightedDegrees[u] == 0.0) {
                danglingMass += current[u];
                continue;
            }

            /*
             * 加权 PageRank 核心：
             * u 对邻居 v 的贡献比例 = weight(u,v) / sumWeight(u)
             * 也就是说，关系越强的邻居会获得越多来自 u 的 PageRank 贡献。
             */
            for (const Edge &edge : adj[u]) {
                int v = edge.to;
                double ratio = edge.weight / weightedDegrees[u];
                next[v] += dampingFactor * current[u] * ratio;
            }
        }

        if (danglingMass > 0.0) {
            double contribution = dampingFactor * danglingMass / n;
            for (double &value : next) {
                value += contribution;
            }
        }

        double diff = 0.0;
        for (int i = 0; i < n; ++i) {
            diff += fabs(next[i] - current[i]);
        }

        current = next;
        result.iterations = iter;
        result.finalDiff = diff;

        if (diff < epsilon) {
            break;
        }
    }

    result.score = current;
    return result;
}

vector<int> sortedByDegree(const Graph &graph) {
    vector<int> indices(graph.nodeCount());
    iota(indices.begin(), indices.end(), 0);

    sort(indices.begin(), indices.end(), [&](int a, int b) {
        double wa = graph.weightedDegree(a);
        double wb = graph.weightedDegree(b);
        if (fabs(wa - wb) > 1e-12) {
            return wa > wb;
        }
        if (graph.degree(a) != graph.degree(b)) {
            return graph.degree(a) > graph.degree(b);
        }
        return graph.getNames()[a] < graph.getNames()[b];
    });

    return indices;
}

vector<int> sortedByPageRank(const Graph &graph, const vector<double> &pageRank) {
    vector<int> indices(graph.nodeCount());
    iota(indices.begin(), indices.end(), 0);

    sort(indices.begin(), indices.end(), [&](int a, int b) {
        if (fabs(pageRank[a] - pageRank[b]) > 1e-12) {
            return pageRank[a] > pageRank[b];
        }
        double wa = graph.weightedDegree(a);
        double wb = graph.weightedDegree(b);
        if (fabs(wa - wb) > 1e-12) {
            return wa > wb;
        }
        if (graph.degree(a) != graph.degree(b)) {
            return graph.degree(a) > graph.degree(b);
        }
        return graph.getNames()[a] < graph.getNames()[b];
    });

    return indices;
}

bool saveDegreeResult(const Graph &graph, const string &filename) {
    ofstream fout(filename, ios::binary);

    if (!fout.is_open()) {
        cerr << "错误：无法写入文件：" << filename << "\n";
        return false;
    }

    const vector<string> &names = graph.getNames();
    vector<int> ranking = sortedByDegree(graph);

    fout << "《三国演义》人物关系网络度数统计结果\n";
    fout << "====================================\n\n";
    fout << "人物节点数量：" << graph.nodeCount() << "\n";
    fout << "关系边数量：" << graph.edgeCount() << "\n\n";

    fout << "排名\t人物\t普通度数\t加权度数\n";
    fout << "------------------------------------\n";

    for (int i = 0; i < static_cast<int>(ranking.size()); ++i) {
        int id = ranking[i];
        fout << (i + 1) << "\t"
             << names[id] << "\t"
             << graph.degree(id) << "\t"
             << fixed << setprecision(2) << graph.weightedDegree(id) << "\n";
    }

    return true;
}

bool savePageRankResult(
    const Graph &graph,
    const PageRankResult &result,
    const string &filename,
    double dampingFactor,
    int maxIterations,
    double epsilon) {

    ofstream fout(filename, ios::binary);

    if (!fout.is_open()) {
        cerr << "错误：无法写入文件：" << filename << "\n";
        return false;
    }

    const vector<string> &names = graph.getNames();
    vector<int> ranking = sortedByPageRank(graph, result.score);

    fout << "《三国演义》人物关系网络 PageRank 分析结果\n";
    fout << "=========================================\n\n";

    fout << "一、图基本信息\n";
    fout << "人物节点数量：" << graph.nodeCount() << "\n";
    fout << "关系边数量：" << graph.edgeCount() << "\n\n";

    fout << "二、PageRank 参数\n";
    fout << "damping factor：" << dampingFactor << "\n";
    fout << "max iteration：" << maxIterations << "\n";
    fout << "epsilon：" << scientific << setprecision(2) << epsilon << "\n";
    fout << "实际迭代次数：" << result.iterations << "\n";
    fout << "最终变化量：" << scientific << setprecision(6) << result.finalDiff << "\n\n";

    fout << "三、PageRank 排名结果\n";
    fout << "排名\t人物\tPageRank值\t普通度数\t加权度数\n";
    fout << "------------------------------------------------\n";

    for (int i = 0; i < static_cast<int>(ranking.size()); ++i) {
        int id = ranking[i];
        fout << (i + 1) << "\t"
             << names[id] << "\t"
             << fixed << setprecision(8) << result.score[id] << "\t"
             << graph.degree(id) << "\t"
             << fixed << setprecision(2) << graph.weightedDegree(id) << "\n";
    }

    fout << "\n四、结果说明\n";
    fout << "普通度数反映人物直接连接的人物数量；加权度数反映人物关系强度之和。\n";
    fout << "加权 PageRank 不仅考虑一个人物连接了多少人，还考虑其相邻人物的重要性，";
    fout << "并按照边权重占该人物所有邻接边权重之和的比例分配 PageRank 贡献。\n";

    return true;
}

void printTopPageRank(const Graph &graph, const PageRankResult &result, int topK) {
    vector<int> ranking = sortedByPageRank(graph, result.score);
    const vector<string> &names = graph.getNames();

    cout << "\nPageRank 前 " << min(topK, static_cast<int>(ranking.size())) << " 名人物：\n";
    cout << "排名\t人物\tPageRank\t普通度数\t加权度数\n";

    for (int i = 0; i < min(topK, static_cast<int>(ranking.size())); ++i) {
        int id = ranking[i];
        cout << (i + 1) << "\t"
             << names[id] << "\t"
             << fixed << setprecision(8) << result.score[id] << "\t"
             << graph.degree(id) << "\t"
             << fixed << setprecision(2) << graph.weightedDegree(id) << "\n";
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    const string inputFile = "data/character_edges_simple.csv";
    const string outputDir = "output";
    const string degreeOutputFile = outputDir + "/degree_result.txt";
    const string pageRankOutputFile = outputDir + "/pagerank_result.txt";

    const double dampingFactor = 0.85;
    const int maxIterations = 100;
    const double epsilon = 1e-8;

    cout << "=========================================\n";
    cout << "《三国演义》人物关系网络 PageRank 分析程序\n";
    cout << "=========================================\n";

    if (!createDirectoryIfNeeded(outputDir)) {
        return 1;
    }

    Graph graph;
    if (!loadGraphFromCsv(inputFile, graph)) {
        return 1;
    }

    if (graph.nodeCount() == 0) {
        cerr << "错误：图中没有任何人物节点，请检查输入数据。\n";
        return 1;
    }

    cout << "\n图结构构建完成。\n";
    cout << "人物节点数量：" << graph.nodeCount() << "\n";
    cout << "关系边数量：" << graph.edgeCount() << "\n";

    PageRankResult pageRank = calculateWeightedPageRank(
        graph,
        dampingFactor,
        maxIterations,
        epsilon);

    cout << "\nPageRank 计算完成。\n";
    cout << "damping factor：" << dampingFactor << "\n";
    cout << "max iteration：" << maxIterations << "\n";
    cout << "epsilon：" << scientific << setprecision(2) << epsilon << "\n";
    cout << "实际迭代次数：" << pageRank.iterations << "\n";
    cout << "最终变化量：" << scientific << setprecision(6) << pageRank.finalDiff << "\n";

    bool degreeSaved = saveDegreeResult(graph, degreeOutputFile);
    bool pageRankSaved = savePageRankResult(
        graph,
        pageRank,
        pageRankOutputFile,
        dampingFactor,
        maxIterations,
        epsilon);

    printTopPageRank(graph, pageRank, 10);

    if (!degreeSaved || !pageRankSaved) {
        return 1;
    }

    cout << "\n结果文件已生成：\n";
    cout << "1. " << degreeOutputFile << "\n";
    cout << "2. " << pageRankOutputFile << "\n";
    cout << "\n程序运行结束。\n";

    return 0;
}

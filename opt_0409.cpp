#include <cstdio>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>

#define DEBUG


using namespace std;

const int MIN_LENGTH = 3;
const int MAX_LENGTH = 7;

typedef int uint_std;
typedef int int_std;

unordered_map<uint_std, uint_std> id2index, index2id;
unordered_map<uint_std, vector<uint_std>> RawGraph;

vector<vector<uint_std>> graph;
vector<vector<uint_std>> new_graph;
vector<vector<uint_std>> ans[8];
vector<uint_std> node_list;
vector<uint_std> used;
vector<unordered_map<int, vector<int>>> jump;

vector<int_std> color, visit;
vector<uint_std> DFN, LOW, group_size, stack;
int tot = 0;
int index = 0, total_ans = 0;

void tarjan(uint_std x, int dep){
    DFN[x] = LOW[x] = ++tot;
    stack[++index] = x;
    visit[x] = 1;
    for (const auto& v: graph[x]){
        if (!DFN[v]){
            tarjan(v, dep + 1);
            LOW[x] = min(LOW[x], LOW[v]);
        } else if (visit[v]){
            LOW[x] = min(LOW[x], DFN[v]);
        }
    }
    if (LOW[x] == DFN[x]){
        group_size[x] = 0;
        do {
            color[stack[index]] = x;
            visit[stack[index]] = 0;
            group_size[x]++;
            index--;
        } while(x != stack[index+1]);
    }
}

//vector<uint_std> & node_list;
void dfs(uint_std u, uint_std head, uint_std len){
    //if (u == head and len > 1){
    //    if (len >= 3){
    //        ans[len].push_back(node_list);
    //        total_ans++;
    //    }
    //    return;
    //}
    if (len >= 5) return;

    auto & Gu = graph[u];
    auto it = lower_bound(Gu.begin(), Gu.end(), head);
    //auto it = Gu.begin();

    for (; it != Gu.end(); ++it){
        const uint_std v = *it;
        if (v < head or used[v] == 1 or v == head){
            continue;
        }

        node_list.push_back(v);
        used[v] = 1;
        if (len >= 0){
            if (jump[head].find(v) != jump[head].end()){
                const auto & jp = jump[head][v];
                for (const auto & mid : jp){
                    if (used[mid] != 0 or mid < head){
                        continue;
                    }
                    auto tmp = node_list;
                    tmp.push_back(mid);
                    ans[len + 3].push_back(tmp);
                    ++total_ans;
                }
            }
        }

        dfs(v, head, len+1);
        used[v] = 0;
        node_list.pop_back();
    }
}

void init_jump(){
    jump.resize(graph.size(), unordered_map<uint_std, vector<int>>());
    for (int i = 0; i < graph.size(); ++i){
        jump[i].rehash(300);
    }
    for (int i = 0; i < graph.size(); ++i){
        for (const auto & v : graph[i]){
            for (const auto & u : graph[v]){
                if (i > u){
                    if (jump[u].find(i) == jump[u].end()){
                        jump[u][i] = vector<uint_std>();
                        //jump[u][i] = vector<uint_std>(30);
                        //jump[u][i].clear();
                    }
                    jump[u][i].push_back(v);
                }
            }
        }
    }
}

int main(){
    #ifdef DEBUG
    char file_name[] = "/mnt/d/downloads/test_data.txt";
    //char file_name[] = "test_data.txt";
    #else
    char file_name[] = "/data/test_data.txt";
    #endif
    FILE * fin = fopen(file_name, "r");
    uint_std a, b, c;

    // Input
    unordered_map<uint_std, int_std> ind;
    while (fscanf(fin, "%u,%u,%u", &a, &b, &c) == 3){
        if (RawGraph.find(a) == RawGraph.end()){
            RawGraph[a] = vector<uint_std> ();
        }
        RawGraph[a].push_back(b);
        ind[b] = 1;
    }

    // Rebuild Graph, and remove v which d_out(v) == 0 
    uint_std count = 0;
    vector<uint_std> ids;
    for (const auto &d : RawGraph) ids.push_back(d.first);
    sort(ids.begin(), ids.end());
    for (const auto a : ids){
        index2id[count] = a;
        id2index[a] = count++;
    }

    const auto graph_size = RawGraph.size();
    //DFN.resize(graph_size);
    //LOW.resize(graph_size);
    graph.resize(graph_size);
    used.resize(graph_size);
    visit.resize(graph_size);
    stack.resize(graph_size * 2);
    color.resize(graph_size);
    group_size.resize(graph_size);
    for (uint_std i = 0; i < graph_size; ++i){
        const auto id = index2id[i];
        const auto & rg = RawGraph[id];
        graph[i] = vector<uint_std> ();
        if (ind.find(id) == ind.end()){
            continue;
        }
        for (const auto & v : rg){
            if (id2index.find(v) == id2index.end()){
                continue;
            }
            graph[i].push_back(id2index[v]);
        }
        sort(graph[i].begin(), graph[i].end());
        //DFN[i] = 0;
        //LOW[i] = 0;
    }

    //for (uint_std i = 0; i < graph_size; ++i){
    //    if (!DFN[i]){
    //        tarjan(i, 0);
    //    }
    //}

    //new_graph.resize(graph.size());
    //for (uint_std i = 0; i < graph_size; ++i){
    //    const auto & g = graph[i];
    //    new_graph[i].clear();
    //    if (group_size[color[i]] < 3){
    //        continue;
    //    }
    //    new_graph[i].resize(graph[i].size());
    //    new_graph[i].clear();
    //    for (const auto & v : g){
    //        if (color[v] == color[i]){
    //            new_graph[i].push_back(v);
    //        }
    //    }
    //}
    //graph = new_graph;

    for (int i = 3; i < 8; ++i){
        ans[i].resize(3000000);
        ans[i].clear();
    }
    //cout << "init..." << endl;
    init_jump();
    //cout << "end init" << endl;
    for (uint_std i = 0; i < graph_size; ++i){
        node_list.clear();
        node_list.push_back(i);
        used[i] = 1;
        dfs(i, i, 0);
        used[i] = 0;
        #ifdef DEBUG
        //printf("d : %d; ans size : %u; edge size: %u\n", i, total_ans, graph[i].size());
        #endif
    }
    //sort(ans.begin(), ans.end());   
    #ifdef DEBUG
        FILE * fout = fopen("new_result.txt", "w");
    #else
        FILE * fout = fopen("/projects/student/result.txt", "w");
    #endif
    fprintf(fout, "%u\n", total_ans);
    //cout << total_ans << endl;
    for (int len = 3; len <= MAX_LENGTH; ++len){
        for (const auto& path: ans[len]){
            //const vector<uint_std> & path = d;
            fprintf(fout, "%u", index2id[path[0]]);
            //cout << path[0] << ' ' << color[path[0]] << ' ' << group_size[color[path[0]]] << endl;
            for (unsigned int i = 1; i < path.size(); ++i){
                fprintf(fout, ",%u", index2id[path[i]]);
                //cout << path[i] << ' ' << color[path[i]] << ' ' << group_size[color[path[i]]] << endl;
            }
            //cout << endl;
            fprintf(fout, "\n");
        }
    }
    fclose(fout);
    fclose(fin);
    return 0;
}


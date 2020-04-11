#include <cstdio>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <string>
#include <cstring>

//#define DEBUG

using namespace std;

const int MIN_LENGTH = 3;
const int MAX_LENGTH = 7;

typedef int uint_std;
typedef int int_std;

unordered_map<uint_std, uint_std> id2index, index2id;
unordered_map<uint_std, vector<uint_std>> RawGraph;

vector<vector<uint_std>> graph;
vector<vector<uint_std>> rev_graph;
vector<vector<uint_std>> new_graph;
vector<vector<uint_std>> ans[8];
vector<string> id_str;
vector<uint_std> node_list;
vector<uint_std> used;
unordered_map<int, vector<pair<int, int>>> jump;

int tot = 0, total_ans = 0;

//vector<uint_std> & node_list;
void dfs(uint_std u, uint_std head, uint_std len){
    //if (u == head and len > 1){
    //    if (len >= 3){
    //        ans[len].push_back(node_list);
    //        total_ans++;
    //    }
    //    return;
    //}
    if (len >= 4) return;

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
            if (jump.find(v) != jump.end()){
                const auto & jp = jump[v];
                for (const auto & mid : jp){
                    if (used[mid.first] != 0 or used[mid.second] or mid.first < head or mid.second < head){
                        continue;
                    }
                    auto tmp = node_list;
                    tmp.push_back(mid.first);
                    tmp.push_back(mid.second);
                    ans[len + 4].push_back(tmp);
                    ++total_ans;
                }
            }
        }

        dfs(v, head, len+1);
        used[v] = 0;
        node_list.pop_back();
    }
}

//void init_jump(){
//    jump.resize(graph.size(), unordered_map<uint_std, vector<int>>());
//    for (int i = 0; i < graph.size(); ++i){
//        jump[i].rehash(300);
//    }
//    for (int i = 0; i < graph.size(); ++i){
//        for (const auto & v : graph[i]){
//            for (const auto & u : graph[v]){
//                if (i > u){
//                    if (jump[u].find(i) == jump[u].end()){
//                        jump[u][i] = vector<uint_std>();
//                        //jump[u][i] = vector<uint_std>(30);
//                        //jump[u][i].clear();
//                    }
//                    jump[u][i].push_back(v);
//                }
//            }
//        }
//    }
//}


vector<uint_std> init_node;
void init_jump(uint_std head){
    //jump = unordered_map<uint_std, vector<pair<int, int>>();
    jump.clear();
    //jump.rehash(300);
    init_node.clear();
    for (const auto & u : rev_graph[head]){
        for (const auto & v : rev_graph[u]){
            for (const auto & mid : rev_graph[v]){
                if (u > head and v > head and mid >= head and u != v){
                    if (jump.find(mid) == jump.end()){
                        jump[mid] = vector<pair<int, int>>();
                        init_node.push_back(mid);
                        //jump[u][i] = vector<uint_std>(30);
                        //jump[u][i].clear();
                    }
                    jump[mid].push_back(make_pair(v, u));
                }
            }
        }
    }
    for (const auto & mid : init_node){
        sort(jump[mid].begin(), jump[mid].end());
    }
}

void get_basic_ans(uint_std head){
    for (const auto mid : jump[head]){
        auto tmp = node_list;
        tmp.push_back(mid.first);
        tmp.push_back(mid.second);
        ans[3].push_back(tmp);
        ++total_ans;
    }
}

char out_buff[8192];
const int MAXN_BUFF_SIZE = 8192;
const int SAFE_BUFF_SIZE = 4096;

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
    id_str.resize(ids.size());
    for (const auto a : ids){
        index2id[count] = a;
        id_str[count] = to_string(a);
        id2index[a] = count++;
    }

    const auto graph_size = RawGraph.size();
    graph.resize(graph_size);
    rev_graph.resize(graph_size);
    used.resize(graph_size);
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
            rev_graph[id2index[v]].push_back(i);
        }
        sort(graph[i].begin(), graph[i].end());
    }
    for (uint_std i = 0; i < graph_size; ++i){
        sort(rev_graph[i].begin(), rev_graph[i].end());
    }

    for (int i = 3; i < 8; ++i){
        ans[i].resize(3000000);
        ans[i].clear();
    }
    //cout << "init..." << endl;
    //init_jump();
    //cout << "end init" << endl;
    for (uint_std i = 0; i < graph_size; ++i){
        node_list.clear();
        node_list.push_back(i);
        init_jump(i);
        get_basic_ans(i);
        dfs(i, i, 0);
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
    //exit(0);
    //sort(ans[3].begin(), ans[3].end());
    int buff_used = 0;
    for (int len = 3; len <= MAX_LENGTH; ++len){
        //sort(ans[len].begin(), ans[len].end());
        for (const auto& path: ans[len]){
            //const vector<uint_std> & path = d;
            //fprintf(fout, "%s", id_str[path[0]]);
            const string & str = id_str[path[0]];
            strcpy(out_buff + buff_used, str.c_str());
            buff_used += str.length();
            for (unsigned int i = 1; i < path.size(); ++i){
                //fprintf(fout, ",%s", id_str[path[i]]);
                out_buff[buff_used++] = ',';
                const string & str = id_str[path[i]];
                strcpy(out_buff + buff_used, str.c_str());
                buff_used += str.length();
            }
            //cout << endl;
            //fprintf(fout, "\n");
            out_buff[buff_used++] = '\n';

            if (buff_used > SAFE_BUFF_SIZE){
                fwrite(out_buff, 1, buff_used, fout);
                buff_used = 0;
            }
        }
        if (buff_used > 0 and len == MAX_LENGTH){
            fwrite(out_buff, 1, buff_used, fout);
            buff_used = 0;
        }
    }
    fclose(fout);
    fclose(fin);
    return 0;
}


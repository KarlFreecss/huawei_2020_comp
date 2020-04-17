/*
TODO:
    1. multi thread;
    2. JUST A SMALL IMPROVE!!! input opt;
    3. THIS METHOD DOESN'T WORK!!! early check, if a node cannot touch head, it should stop early;
    4. enumerate start depends on head!
    5. merge single path
    6. using topo delete useless node
    7. add tarjan again
*/
//#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <string>
#include <cstring>
#include <mutex>
#include <thread>
#include <queue>

#include <ctime>

#define DEBUG

using namespace std;

const int MIN_LENGTH = 3;
const int MAX_LENGTH = 7;
const int THREAD_NUM = 4;

//typedef int int_std;
typedef int int_std;

mutex mtx;

unordered_map<int_std, int_std> id2index, index2id;
unordered_map<int_std, vector<int_std>> RawGraph;

vector<vector<int_std>> graph;
vector<vector<int_std>> rev_graph;
vector<vector<vector<int_std>>> ans_pool[8];
vector<string> id_str;
vector<int_std> undo;

vector<int_std> used_pool[THREAD_NUM];
vector<vector<pair<int, int>>> jump_pool[THREAD_NUM];

inline
void quick_jump(int_std head, 
                int_std v,
                vector<vector<pair<int, int>>> & jump,
                const vector<int_std> & node_list,
                const vector<int_std> & used){
    const int len = node_list.size() + 2;
    auto & ans = ans_pool[len][head];
    const auto & jp = jump[v];
    for (const auto & mid : jp){
        if (used[mid.first] == 0 and used[mid.second] == 0 and mid.first >= head and mid.second >= head){
            auto tmp = node_list;
            tmp.push_back(mid.first);
            tmp.push_back(mid.second);
            ans.push_back(tmp);
        }
    }
}

void search(int_std head,
            vector<vector<pair<int, int>>> & jump,
            const vector<int_std> & jump_update_flag,
            vector<int_std> & used){
    vector<int_std> node_list(MAX_LENGTH);
    node_list.clear();
    node_list.push_back(head);
    if (jump_update_flag[head] == head and jump[head].size() != 0) quick_jump(head, head, jump, node_list, used);
    used[head]  = 1;
    for (const auto u : graph[head]) {
        if (u < head or used[u]) continue;
        node_list.push_back(u);
        used[u] = 1;
        //quick_jump(head, u, jump, node_list, used);
        if (jump_update_flag[u] == head and jump[u].size() != 0) quick_jump(head, u, jump, node_list, used);
        for (const auto v : graph[u]) {
            if (v < head or used[v]) continue;
            node_list.push_back(v);
            used[v] = 1;
            //quick_jump(head, v, jump, node_list, used);
            if (jump_update_flag[v] == head and jump[v].size() != 0) quick_jump(head, v, jump, node_list, used);
            for (const auto k : graph[v]) {
                if (k < head or used[k]) continue;
                node_list.push_back(k);
                used[k] = 1;
                //quick_jump(head, k, jump, node_list, used);
                if (jump_update_flag[k] == head and jump[k].size() != 0) quick_jump(head, k, jump, node_list, used);
                for (const auto l : graph[k]) {
                    if (l < head or used[l]) continue;
                    node_list.push_back(l);
                    used[l] = 1;
                    //quick_jump(head, l, jump, node_list, used);
                    if (jump_update_flag[l] == head and jump[l].size() != 0) quick_jump(head, l, jump, node_list, used);
                    used[l] = 0;
                    node_list.pop_back();
                }
                used[k] = 0;
                node_list.pop_back();
            }
            used[v] = 0;
            node_list.pop_back();
        }
        used[u] = 0;
        node_list.pop_back();
    }
    used[head] = 0;
}

void init_jump(int_std head, 
            vector<vector<pair<int, int>>> & jump, 
            vector<int_std> & jump_update_flag, 
            vector<int_std> & init_node){
    //jump = unordered_map<int_std, vector<pair<int, int>>();
    init_node.clear();
    for (const auto & u : rev_graph[head]){
        if (u <= head) continue;
        for (const auto & v : rev_graph[u]){
            if (v <= head or u == v) continue;
            for (const auto & mid : rev_graph[v]){
                //if (u > head and v > head and mid >= head and u != v){
                if (mid >= head and mid != u){
                    //if (jump.find(mid) == jump.end()){
                    if (jump_update_flag[mid] != head){
                        jump_update_flag[mid] = head;
                        jump[mid].clear();
                        init_node.push_back(mid);
                        //jump[u][i] = vector<int_std>(30);
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

void BFS(int_std head, vector<int_std> & visit, vector<int_std> & dist){
    visit[head] = head;
    dist[head] = 0;
    queue<int_std> que;
    que.push(head);

    while (!que.empty()){
        int_std x = que.front();
        que.pop();

        for (const auto & u : rev_graph[x]){
            if (u >= head and visit[u] != head){
                visit[u] = head;
                dist[u] = dist[x] + 1;
                if (dist[u] <= 7){
                    que.push(u);
                }
            }
        }
    }
    
}

void run_job(int_std thread_id, int_std graph_size){
    vector<int_std> init_node;
    vector<int_std> jump_update_flag(graph_size, -1);
    jump_pool[thread_id].resize(graph_size);
    //for (int i = 0; i < graph_size; ++i){
    //    jump_pool[thread_id][i].resize(16);
    //}
    //vector<int_std> visit(graph_size);
    //vector<int_std> dist(graph_size);
    for (int i = thread_id; i < graph_size; i+=THREAD_NUM){
        //int need_to_do = false;
        //mtx.lock();
        //if (undo[i] == 1){
        //    need_to_do = true;
        //    undo[i] = 0;
        //}
        //mtx.unlock();
        //if (need_to_do){
        if (i % THREAD_NUM == thread_id){
            init_jump(i, jump_pool[thread_id], jump_update_flag, init_node);
            search(i, jump_pool[thread_id], jump_update_flag, used_pool[thread_id]);
        }
    }
}

const int MAX_BUFF_SIZE = 4096 * 1024;
const int SAFE_BUFF_SIZE = 2048 * 1024;
const int LEAST_DATA_LEFT = 1024;
char out_buff[MAX_BUFF_SIZE];
char in_buff[MAX_BUFF_SIZE];

const int MAX_DATA_SIZE = 560000;
vector<int_std> A(MAX_DATA_SIZE), B(MAX_DATA_SIZE), C(MAX_DATA_SIZE);

inline
int_std get_num(char buff[], int_std & used){
    while (buff[used] < '0' or buff[used] > '9') ++used;
    int_std ret = buff[used++] - '0';
    while (buff[used] >= '0' && buff[used] <= '9') {
        ret = ret * 10 + buff[used++] - '0';
    }
    return ret;
}

void quick_input(char * file_name){
    FILE * fin = fopen(file_name, "r");
    A.clear(); B.clear(); C.clear();
    int_std is_finished = false;
    int_std used = 0;
    int_std left_data_size = 0;
    while (false == is_finished){
        int_std read_size = fread(in_buff + left_data_size, 1, SAFE_BUFF_SIZE, fin);
        left_data_size += read_size;
        if (read_size < SAFE_BUFF_SIZE) is_finished = true;
        while (left_data_size - used > LEAST_DATA_LEFT){
            A.push_back(get_num(in_buff, used)); 
            B.push_back(get_num(in_buff, used)); 
            C.push_back(get_num(in_buff, used)); 
        }
        strcpy(in_buff, in_buff + used);
        left_data_size = left_data_size - used;
        used = 0;
    }
    while (left_data_size - used > 3){
        A.push_back(get_num(in_buff, used)); 
        B.push_back(get_num(in_buff, used)); 
        C.push_back(get_num(in_buff, used)); 
    }
    fclose(fin);
}

void normal_input(char * file_name){
    FILE * fin = fopen(file_name, "r");
    A.clear(); B.clear(); C.clear();
    int_std a, b, c;
    while (fscanf(fin, "%u,%u,%u", &a, &b, &c) == 3){
        A.push_back(a); 
        B.push_back(b); 
        C.push_back(c); 
    }
    fclose(fin);
}

int main(){
    auto beg_t = clock();
    #ifdef DEBUG
	//char file_name[] = "D:/downloads/test_data.txt";
    char file_name[] = "/mnt/d/downloads/test_data.txt";
    //char file_name[] = "test_data.txt";
    #else
    char file_name[] = "/data/test_data.txt";
    #endif
    //FILE * fin = fopen(file_name, "r");

    //normal_input(file_name);
    quick_input(file_name);
    //cout << A.size() << endl;

    // Input
    unordered_map<int_std, int_std> ind;
    //while (fscanf(fin, "%u,%u,%u", &a, &b, &c) == 3){
    for (int i = 0; i < A.size(); ++i){
        int_std a = A[i], b = B[i], c = C[i];
        
        if (RawGraph.find(a) == RawGraph.end()){
            RawGraph[a] = vector<int_std> ();
        }
        RawGraph[a].push_back(b);
        ind[b] = 1;
    }
    //exit(0);

    // Rebuild Graph, and remove v which d_out(v) == 0 
    int_std count = 0;
    vector<int_std> ids;
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
    undo.resize(graph_size);
    for (int i = 0; i < THREAD_NUM; ++i){
        used_pool[i].resize(graph_size);
    }
    for (int_std i = 0; i < graph_size; ++i){
        const auto id = index2id[i];
        const auto & rg = RawGraph[id];
        graph[i] = vector<int_std> ();
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
        undo[i] = true;
        sort(graph[i].begin(), graph[i].end());
    }
    for (int_std i = 0; i < graph_size; ++i){
        sort(rev_graph[i].begin(), rev_graph[i].end());
    }

    for (int i = MIN_LENGTH; i <= MAX_LENGTH; ++i){
        ans_pool[i].resize(graph_size);
    }
    //exit(0);

    //for (int_std i = 0; i < graph_size; ++i){
    //    init_jump(jump_pool[0], i);
    //    search(i, jump_pool[0], used_pool[0]);
    //    #ifdef DEBUG
    //    //printf("d : %d; ans size : %u; edge size: %u\n", i, total_ans, graph[i].size());
    //    #endif
    //}
    //cout << double(clock() - beg_t)/CLOCKS_PER_SEC << endl;
    thread threads[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; ++i){
        threads[i] = thread(run_job, i, graph_size);
    }
    for (int i = 0; i < THREAD_NUM; ++i){
        threads[i].join();
    }
    //cout << double(clock() - beg_t)/CLOCKS_PER_SEC << endl;

    //sort(ans.begin(), ans.end());   
    #ifdef DEBUG
        FILE * fout = fopen("new_result.txt", "w");
    #else
        FILE * fout = fopen("/projects/student/result.txt", "w");
    #endif
    int_std total_ans = 0;
    for (int len = 3; len <= MAX_LENGTH; ++len){
        for (int i = 0; i < graph_size; ++i){
            total_ans += ans_pool[len][i].size();
        }
    }
    fprintf(fout, "%u\n", total_ans);
    //cout << total_ans << endl;
    //exit(0);
    //sort(ans[3].begin(), ans[3].end());
    int buff_used = 0;
    for (int len = 3; len <= MAX_LENGTH; ++len){
        //sort(ans[len].begin(), ans[len].end());
        const auto & ans = ans_pool[len];
        for (int i = 0; i < graph_size; ++i){
            for (const auto& path: ans[i]){
                //const vector<int_std> & path = d;
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
        }
        if (buff_used > 0 and len == MAX_LENGTH){
            fwrite(out_buff, 1, buff_used, fout);
            buff_used = 0;
        }
    }
    fclose(fout);
    //fclose(fin);
    //cout << (clock() - beg_t)/CLOCKS_PER_SEC << endl;
    //cout << time_count * 1. / CLOCKS_PER_SEC << endl;
    return 0;
}


/*
TODO:
    1.  WORK, BUT NOT IDEAL! multi thread;
    2.  JUST A SMALL IMPROVE!!! input opt;
    3.  THIS METHOD DOESN'T WORK!!! early check, if a node cannot touch head, it should stop early;
    4.  enumerate start depends on head!
    5.  merge single path
    6.  UNKNOWN! add tarjan again
    7.  WORK! ans_pool just record ans index
    8.  MMAP IO (write)
    9.  multi thread output
    10. multi thread preprocess
    11. rewrite vector, because clear is so slow()
    12.
*/
#define _CRT_SECURE_NO_WARNINGS
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

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ctime>

#define DEBUG

using namespace std;

const int MIN_LENGTH = 3;
const int MAX_LENGTH = 7;
const int THREAD_NUM = 4;
const int MAX_ANS_LEN = 3000000 * 2;
const int MAX_ID_NUM = 280000 * 2;

//typedef int int_std;
typedef int int_std;

mutex mtx;

unordered_map<int_std, int_std> id2index, index2id;
unordered_map<int_std, vector<int_std>> RawGraph;

vector<vector<int_std>> graph, new_graph;
vector<vector<int_std>> rev_graph;
vector<vector<int_std>> ans_pool[8];
vector<vector<pair<int, int>>> jump_pool[THREAD_NUM];

vector<int_std> handle_thread_id;
vector<int_std> real_ans[THREAD_NUM];
vector<string> id_str;
vector<int_std> id_len;
//vector<int_std> undo;

vector<bool> used_pool[THREAD_NUM];

vector<int_std> DFN, LOW, visit, stack, group_size, color;
int tot, t_index;
void tarjan(int_std x, int dep){
    DFN[x] = LOW[x] = ++tot;
    stack[++t_index] = x;
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
            color[stack[t_index]] = x;
            visit[stack[t_index]] = 0;
            group_size[x]++;
            t_index--;
        } while(x != stack[t_index+1]);
    }
}

inline
void quick_jump(int_std head, 
                int_std v,
                vector<vector<pair<int, int>>> & jump,
                const vector<int_std> & node_list,
                const vector<bool> & used,
                int_std thread_id){
    const int len = node_list.size() + 2;
    auto & ans = ans_pool[len][head];
    const auto & jp = jump[v];
    for (const auto & mid : jp){
        if (used[mid.first] == 0 and used[mid.second] == 0){
            ans.push_back(real_ans[thread_id].size());
            for (const auto & u : node_list){
                real_ans[thread_id].push_back(u);
            }
            real_ans[thread_id].push_back(mid.first);
            real_ans[thread_id].push_back(mid.second);
        }
    }
}

void search(int_std head,
            vector<vector<pair<int, int>>> & jump,
            const vector<int_std> & jump_update_flag,
            vector<bool> & used,
            int_std thread_id){
    vector<int_std> node_list(MAX_LENGTH);
    node_list.clear();
    node_list.push_back(head);
    if (jump_update_flag[head] == head and jump[head].size() != 0) quick_jump(head, head, jump, node_list, used, thread_id);
    used[head]  = 1;
    auto itr_u = lower_bound(graph[head].begin(), graph[head].end(), head);
    for (;itr_u != graph[head].end(); ++itr_u){
        const auto u = *itr_u;
        if (used[u]) continue;
        node_list.push_back(u);
        used[u] = 1;
        //quick_jump(head, u, jump, node_list, used);
        if (jump_update_flag[u] == head and jump[u].size() != 0) quick_jump(head, u, jump, node_list, used, thread_id);
        auto itr_v = lower_bound(graph[u].begin(), graph[u].end(), head);
        for (;itr_v != graph[u].end(); ++itr_v){
            const auto v = *itr_v;
            if (used[v]) continue;
            node_list.push_back(v);
            used[v] = 1;
            //quick_jump(head, v, jump, node_list, used);
            if (jump_update_flag[v] == head and jump[v].size() != 0) quick_jump(head, v, jump, node_list, used, thread_id);
            //auto itr_k = lower_bound(graph[v].begin(), graph[v].end(), head);
			auto itr_k = graph[v].begin();
			for (; itr_k != graph[v].end(); ++itr_k) { if (*itr_k >= head) break; }
            for (;itr_k != graph[v].end(); ++itr_k){
                const auto k = *itr_k;
                if (used[k]) continue;
                node_list.push_back(k);
                used[k] = 1;
                //quick_jump(head, k, jump, node_list, used);
                if (jump_update_flag[k] == head and jump[k].size() != 0) quick_jump(head, k, jump, node_list, used, thread_id);
                //auto itr_l = lower_bound(graph[k].begin(), graph[k].end(), head);
				auto itr_l = graph[k].begin();
				for (; itr_l != graph[k].end(); ++itr_l) { if (*itr_l >= head) break;}
				for (;itr_l != graph[k].end(); ++itr_l){
                    const auto l = *itr_l;
                    if (used[l]) continue;
                    //quick_jump(head, l, jump, node_list, used);
					if (jump_update_flag[l] == head and jump[l].size() != 0) {
						node_list.push_back(l);
						used[l] = 1;
						quick_jump(head, l, jump, node_list, used, thread_id);
						used[l] = 0;
						node_list.pop_back();
					}
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
            handle_thread_id[i] = thread_id;
            search(i, jump_pool[thread_id], jump_update_flag, used_pool[thread_id], thread_id);
        }
        //cout << i << ' ' << real_ans[thread_id].size() << endl;
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

inline
void int2chars(int_std num, char * ret, int & len){
    len = 0;
    ret[0] = num % 10 + '0';
    num /= 10;
    while (num > 0){
        ret[++len] = num % 10 + '0';
        num /= 10;
    }
}

inline
void write_count(const int graph_size, int_std & total_ans, int_std & out_size, 
                    vector<int_std> & ans_len, vector<int_std> & ans_head, vector<int_std> & ans_write_addr){
    total_ans = 0;
    out_size = 0;
    for (int len = 3; len <= MAX_LENGTH; ++len){
        const auto & ans = ans_pool[len];
        for (int i = 0; i < graph_size; ++i){
            ans_len.push_back(len);
            ans_head.push_back(i);
            ans_write_addr.push_back(out_size);
            int_std thread_id = handle_thread_id[i];
            total_ans += ans[i].size();
            const auto & real_a = real_ans[thread_id];
            for (const auto& start_point: ans[i]){
                out_size += id_len[real_a[start_point]];
                for (unsigned int i = start_point + 1; i < start_point + len; ++i){
                    out_size += id_len[real_a[i]] + 1;
                }
                ++out_size;
            }
        }
    }
    out_size += to_string(total_ans).length() + 1;
}

void mmap_multi_thread_writer(const int_std writer_id,
                              char * out_buff,
                              const vector<int_std> & ans_len, 
                              const vector<int_std> & ans_head, 
                              const vector<int_std> & ans_write_addr,
                              const int_std ans_num_offset){
    const int ans_len_size = ans_len.size();
    //const int handle_num = (ans_len_size - 1) / THREAD_NUM + 1;
    //const int begin = handle_num * writer_id;
    //const int end = min(ans_len_size, handle_num * (writer_id + 1));
    for (int aidx = writer_id; aidx < ans_len_size; aidx+=THREAD_NUM){
    //for (int aidx = begin; aidx < end; ++aidx){
        const int i = ans_head[aidx];
        const int len = ans_len[aidx];
        int write_offset = ans_write_addr[aidx] + ans_num_offset;
        int_std thread_id = handle_thread_id[i];
        const auto & real_a = real_ans[thread_id];
        const auto & ans = ans_pool[len];
        for (const auto& start_point: ans[i]){
            const string & str = id_str[real_a[start_point]];
            strcpy(out_buff + write_offset, str.c_str());
            write_offset += str.length();
            for (unsigned int i = start_point + 1; i < start_point + len; ++i){
                out_buff[write_offset++] = ',';
                const string & str = id_str[real_a[i]];
                strcpy(out_buff + write_offset, str.c_str());
                write_offset += str.length();
            }
            out_buff[write_offset++] = '\n';
        }
    }
}

void mmap_write_data(char file_name[], const int graph_size){
    int_std total_ans, out_size;
    vector<int_std> ans_len, ans_head, ans_write_addr;
    ans_len.reserve(MAX_ANS_LEN);
    ans_head.reserve(MAX_ANS_LEN);
    ans_write_addr.reserve(MAX_ANS_LEN);
    write_count(graph_size, total_ans, out_size, ans_len, ans_head, ans_write_addr);
    //cout << "file size should be: " << out_size << endl;

    int fd = open(file_name, O_CREAT|O_RDWR|O_TRUNC, 0666);
    int flag = ftruncate(fd, out_size);
    char * out_buff = (char*) mmap(NULL, out_size, PROT_WRITE, MAP_SHARED, fd, 0);
    //char * out_buff = (char*) mmap(NULL, out_size, PROT_WRITE, MAP_LOCKED, fd, 0);

    string total_ans_str = to_string(total_ans);
    int buff_used = total_ans_str.length();
    strcpy(out_buff, total_ans_str.c_str());
    out_buff[buff_used++] = '\n';
    int ans_num_offset = buff_used;
    #ifdef DEBUG
    cout << total_ans << endl;
    #endif

    thread threads[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; ++i){
        threads[i] = thread(mmap_multi_thread_writer, i, out_buff, ans_len, ans_head, ans_write_addr, ans_num_offset);
    }
    for (int i = 0; i < THREAD_NUM; ++i){
        threads[i].join();
    }
    close(fd);
}

void normal_write_data(char file_name[], const int graph_size){
    FILE * fout = fopen(file_name, "w");
    int_std total_ans = 0;
    for (int len = 3; len <= MAX_LENGTH; ++len){
        const auto & ans = ans_pool[len];
        for (int i = 0; i < graph_size; ++i){
            total_ans += ans_pool[len][i].size();
        }
    }
    fprintf(fout, "%u\n", total_ans);
    #ifdef DEBUG
    cout << total_ans << endl;
    #endif
    //exit(0);
    //sort(ans[3].begin(), ans[3].end());
    int buff_used = 0;
    for (int len = 3; len <= MAX_LENGTH; ++len){
        //sort(ans[len].begin(), ans[len].end());
        const auto & ans = ans_pool[len];
        for (int i = 0; i < graph_size; ++i){
            int_std thread_id = handle_thread_id[i];
            const auto & real_a = real_ans[thread_id];
            for (const auto& start_point: ans[i]){
                const string & str = id_str[real_a[start_point]];
                strcpy(out_buff + buff_used, str.c_str());
                buff_used += str.length();
                for (unsigned int i = start_point + 1; i < start_point + len; ++i){
                    out_buff[buff_used++] = ',';
                    const string & str = id_str[real_a[i]];
                    strcpy(out_buff + buff_used, str.c_str());
                    buff_used += str.length();
                    //const int_std id = index2id[real_a[i]];
                    //char str[16];
                    //int len = 0;
                    //int2chars(id, str, len);
                    //for (int i = len; i >= 0; --i){
                    //    out_buff[buff_used++] = str[i];
                    //}
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
}

int main(){
    #ifdef DEBUG
    //auto beg_t = clock();
	//char in_file_name[] = "D:/downloads/test_data.txt";
    char in_file_name[] = "/mnt/d/downloads/test_data.txt";
    //char in_file_name[] = "test_data.txt";
    char out_file_name[] = "new_result.txt";
    #else
    char in_file_name[] = "/data/test_data.txt";
    char out_file_name[] = "/projects/student/result.txt";
    #endif
    //FILE * fin = fopen(file_name, "r");

    //normal_input(file_name);
    quick_input(in_file_name);
    //cout << A.size() << endl;

    // Input
    RawGraph.reserve(MAX_DATA_SIZE);
    RawGraph.rehash(MAX_DATA_SIZE);
    //while (fscanf(fin, "%u,%u,%u", &a, &b, &c) == 3){
    for (int i = 0; i < A.size(); ++i){
        int_std a = A[i], b = B[i], c = C[i];
        
        if (RawGraph.find(a) == RawGraph.end()){
            RawGraph[a] = vector<int_std> ();
        }
        RawGraph[a].push_back(b);
    }
    //exit(0);

    // Rebuild Graph, and remove v which d_out(v) == 0 
    int_std count = 0;
    vector<int_std> ids;
    for (const auto &d : RawGraph) ids.push_back(d.first);
    const auto graph_size = RawGraph.size();
    graph.resize(graph_size);
    handle_thread_id.reserve(graph_size);
    rev_graph.resize(graph_size);
    sort(ids.begin(), ids.end());

    id_str.resize(ids.size());
    id_len.resize(ids.size());
    id2index.reserve(MAX_ID_NUM);
    id2index.rehash(MAX_ID_NUM);
    index2id.reserve(MAX_ID_NUM);
    index2id.rehash(MAX_ID_NUM);
    for (const auto a : ids){
        index2id[count] = a;
        id_str[count] = to_string(a);
        id_len[count] = id_str[count].length();
        id2index[a] = count++;
    }

    for (int i = 0; i < graph_size; ++i){
        graph[i].reserve(10);
        rev_graph[i].reserve(10);
    }
    //undo.resize(graph_size);
    for (int i = 0; i < THREAD_NUM; ++i){
        real_ans[i].reserve(MAX_ANS_LEN);
        used_pool[i].resize(graph_size);
    }
    for (int_std i = 0; i < graph_size; ++i){
        const auto id = index2id[i];
        const auto & rg = RawGraph[id];
        for (const auto & v : rg){
            if (id2index.find(v) == id2index.end()){
                continue;
            }
            graph[i].push_back(id2index[v]);
            rev_graph[id2index[v]].push_back(i);
        }
        //undo[i] = true;
    }
    for (int_std i = 0; i < graph_size; ++i){
        sort(graph[i].begin(), graph[i].end());
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
    LOW = vector<int_std> (graph_size, 0);
    DFN = vector<int_std> (graph_size, 0);
    visit = vector<int_std> (graph_size, 0);
    stack = vector<int_std> (graph_size, 0);
    group_size = vector<int_std> (graph_size, 0);
    color = vector<int_std> (graph_size, 0);
    t_index = 0;
    tot = 0;
    for (int_std i = 0; i < graph_size; ++i){
        if (!DFN[i]){
            tarjan(i, 0);
        }
    }

    new_graph.resize(graph.size());
    vector<int_std> in(graph_size, 0), outd(graph_size, 0);
    for (int_std i = 0; i < graph_size; ++i){
        const auto & g = graph[i];
        if (group_size[color[i]] < 3){
            continue;
        }
        new_graph[i].reserve(graph[i].size());
        //new_graph[i].clear();
        for (const auto & v : g){
            if (color[v] == color[i]){
                new_graph[i].push_back(v);
                //in[v]++;
                //outd[i]++;
            }
        }
    }
    #ifdef DEBUG
    int_std single_path = 0;
    for (int i = 0; i < graph_size; ++i){
        if ((in[i] == 1 and outd[i] == 1) or in[i] == 0 or outd[i] == 0){
            single_path++;
        }
    }
    cout << graph_size << ' ' << single_path << endl;
    #endif
    graph = new_graph;

    thread threads[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; ++i){
        threads[i] = thread(run_job, i, graph_size);
    }
    for (int i = 0; i < THREAD_NUM; ++i){
        threads[i].join();
    }

    //sort(ans.begin(), ans.end());   
    //normal_write_data(out_file_name, graph_size);
    mmap_write_data(out_file_name, graph_size);
    //fclose(fin);
    //cout << time_count * 1. / CLOCKS_PER_SEC << endl;
    return 0;
}


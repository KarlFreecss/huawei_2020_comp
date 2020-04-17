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
    12. more detail multi process
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
#include <thread>
#include <queue>
#include <atomic>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ctime>

//#define DEBUG

using namespace std;

const int MIN_LENGTH = 3;
const int MAX_LENGTH = 7;
const int THREAD_NUM = 4;
const int MAX_ANS_LEN = 3000000;
const int MAX_ID_NUM = 280000;
const int MAX_DATA_SIZE = 280000;

//typedef int int_std;
typedef int int_std;

unordered_map<int_std, int_std> id2index;
vector<int_std> index2id;
vector<vector<int_std>> RawGraph;

//vector<vector<int_std>> graph, new_graph;
vector<int_std> graph, in_degree, out_degree, first_edge, last_edge;
vector<vector<int_std>> rev_graph;
vector<vector<int_std>> ans_pool[8];

vector<int_std> handle_thread_id;
vector<int_std> real_ans[THREAD_NUM];
vector<string> id_str;
vector<int_std> id_len;
vector<int_std> undo;

inline
void quick_jump(int_std head, 
                int_std v,
                vector<vector<pair<int, int>>> & jump,
                const vector<int_std> & node_list,
                int_std thread_id){
    const int len = node_list.size() + 2;
    auto & ans = ans_pool[len][head];
    const auto & jp = jump[v];
    register int_std b = -1, c = -1, d = -1;
    if (len > 3){
        b = node_list[1];
        if (len > 4){
            c = node_list[2];
            if (len > 5){
                d = node_list[3];
            }
        }
    }
    for (const auto & mid : jp){
        //if (used[mid.first] == 0 and used[mid.second] == 0){
        if (mid.first != b and 
            mid.second != b and
            mid.first != c and 
            mid.second != c and 
            mid.first != d and
            mid.second != d){
            ans.emplace_back(real_ans[thread_id].size());
            for (const auto & u : node_list){
                real_ans[thread_id].emplace_back(u);
            }
            real_ans[thread_id].emplace_back(mid.first);
            real_ans[thread_id].emplace_back(mid.second);
        }
    }
}

void search(int_std head,
            vector<vector<pair<int, int>>> & jump,
            const vector<int_std> & jump_update_flag,
            int_std thread_id,
            vector<int_std> & first_edge){
    vector<int_std> node_list(MAX_LENGTH);
    node_list.clear();
    node_list.emplace_back(head);
    if (jump_update_flag[head] == head) quick_jump(head, head, jump, node_list, thread_id);
    auto itr_u = first_edge[head];
    const auto itr_u_end = last_edge[head];
    for (;itr_u < itr_u_end; ++itr_u) { if (graph[itr_u] > head) break;}
    first_edge[head] = itr_u;
    for (;itr_u < itr_u_end; ++itr_u){
        const auto u = graph[itr_u];
        node_list.emplace_back(u);
        if (jump_update_flag[u] == head) quick_jump(head, u, jump, node_list, thread_id);
        auto itr_v = first_edge[u];
        const auto itr_v_end = last_edge[u];
        for (; itr_v < itr_v_end; ++itr_v){ if (graph[itr_v] > head) break;}
        first_edge[u] = itr_v;
        for (;itr_v < itr_v_end; ++itr_v){
            const auto v = graph[itr_v];
            node_list.emplace_back(v);
            if (jump_update_flag[v] == head) quick_jump(head, v, jump, node_list, thread_id);
			auto itr_k = first_edge[v];
            const auto itr_k_end = last_edge[v];
            for (; itr_k < itr_k_end; ++itr_k){ if (graph[itr_k] > head) break;}
            first_edge[v] = itr_k;
            for (;itr_k < itr_k_end; ++itr_k){
                const auto k = graph[itr_k];
                if (k == u) continue;
                node_list.emplace_back(k);
                if (jump_update_flag[k] == head) quick_jump(head, k, jump, node_list, thread_id);
				auto itr_l = first_edge[k];
                const auto itr_l_end = last_edge[k];
				for (; itr_l < itr_l_end; ++itr_l) {
                    if (graph[itr_l] > head) {
                        first_edge[k] = itr_l;
                        break;
                    }
                }
				for (;itr_l < itr_l_end; ++itr_l){
                    const auto l = graph[itr_l];
					if (l != u and l != v and jump_update_flag[l] == head) {
						node_list.emplace_back(l);
						quick_jump(head, l, jump, node_list, thread_id);
						node_list.pop_back();
					}
                }
                node_list.pop_back();
            }
            node_list.pop_back();
        }
        node_list.pop_back();
    }
    node_list.pop_back();
}

int init_jump(int_std head, 
            vector<vector<pair<int, int>>> & jump, 
            vector<int_std> & jump_update_flag, 
            vector<int_std> & init_node){
    int jump_num = 0;
    init_node.clear();
    for (int itr_u = rev_graph[head].size() - 1; itr_u >= 0 && rev_graph[head][itr_u] > head; --itr_u){
        const auto u = rev_graph[head][itr_u];
        const auto & rev_graph_u = rev_graph[u];
        for (int itr_v = rev_graph_u.size() - 1; itr_v >= 0 && rev_graph_u[itr_v] > head; --itr_v){
            const auto v = rev_graph_u[itr_v];
            const auto & rev_graph_v = rev_graph[v];
            for (int itr_mid = rev_graph[v].size() - 1; itr_mid >= 0 && rev_graph_v[itr_mid] >= head; --itr_mid){
                const auto mid = rev_graph_v[itr_mid];
                if (mid != u){
                    if (jump_update_flag[mid] != head){
                        jump_update_flag[mid] = head;
                        jump[mid].clear();
                        init_node.emplace_back(mid);
                        jump_num+=1;
                    }
                    jump[mid].emplace_back(make_pair(v, u));
                }
            }
        }
    }
    for (const auto & mid : init_node){
        sort(jump[mid].begin(), jump[mid].end());
    }
    return jump_num;
}

atomic_long handle_num(-1);
void run_job(int_std thread_id, int_std graph_size){
    vector<int_std> init_node;
    vector<int_std> thread_first_edge(first_edge);
    vector<int_std> jump_update_flag(graph_size, -1);
    vector<vector<pair<int, int>>> jump_pool(graph_size);
    vector<int_std> node_list;

    real_ans[thread_id].reserve(MAX_ANS_LEN);
    //for (int i = 0; i < graph_size; ++i){
    //    jump_pool[thread_id][i].resize(16);
    //}
    //vector<int_std> visit(graph_size);
    //vector<int_std> dist(graph_size);
    //for (int i = thread_id; i < graph_size; i+=THREAD_NUM){
    for (int t = 0; t < graph_size; ++t){
        //int need_to_do = false;
        //mtx.lock();
        //if (undo[i] == 1){
        //    need_to_do = true;
        //    undo[i] = 0;
        //}
        //mtx.unlock();
        //if (need_to_do){
        int i = handle_num += 1;
        if (i >= graph_size){break;}
        //if (i % THREAD_NUM == thread_id){
        const int jump_num = init_jump(i, jump_pool, jump_update_flag, init_node);
        if (jump_num > 0){
            handle_thread_id[i] = thread_id;
            search(i, jump_pool, jump_update_flag, thread_id, thread_first_edge);
        }
        //}
        //cout << i << ' ' << real_ans[thread_id].size() << endl;
    }
}

const int MAX_BUFF_SIZE = 4096 * 1024;
const int SAFE_BUFF_SIZE = 2048 * 1024;
const int LEAST_DATA_LEFT = 1024;
char in_buff[MAX_BUFF_SIZE];

vector<pair<int_std, int_std>> tD(MAX_DATA_SIZE);

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
    tD.clear();
    int_std is_finished = false;
    int_std used = 0;
    int_std left_data_size = 0;
    while (false == is_finished){
        int_std read_size = fread(in_buff + left_data_size, 1, SAFE_BUFF_SIZE, fin);
        left_data_size += read_size;
        if (read_size < SAFE_BUFF_SIZE) is_finished = true;
        while (left_data_size - used > LEAST_DATA_LEFT){
            const int a = get_num(in_buff, used);
            const int b = get_num(in_buff, used);
            const int c = get_num(in_buff, used);
            if (a == b){continue;}
            tD.emplace_back(make_pair(a, b));
        }
        strcpy(in_buff, in_buff + used);
        left_data_size = left_data_size - used;
        used = 0;
    }
    while (left_data_size - used > 3){
        const int a = get_num(in_buff, used);
        const int b = get_num(in_buff, used);
        const int c = get_num(in_buff, used);
        if (a == b){continue;}
        tD.emplace_back(make_pair(a, b));
    }
    fclose(fin);
    sort(tD.begin(), tD.end());
}

inline
void write_count(const int graph_size, int_std & total_ans, int_std & out_size, 
                    vector<int_std> & ans_len, vector<int_std> & ans_head, vector<int_std> & ans_write_addr){
    total_ans = 0;
    out_size = 0;
    for (int len = 3; len <= MAX_LENGTH; ++len){
        const auto & ans = ans_pool[len];
        for (int i = 0; i < graph_size; ++i){
            ans_len.emplace_back(len);
            ans_head.emplace_back(i);
            ans_write_addr.emplace_back(out_size);
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

inline int_std str_copy(char *dst, const string &str){
    const char * raw = str.c_str();
    const int_std sz = str.size();
    memcpy(dst, raw, sz);
    return sz;
}

atomic_long write_num(-1);
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
    //for (int aidx = writer_id; aidx < ans_len_size; aidx+=THREAD_NUM){
    //for (int aidx = begin; aidx < end; ++aidx){
    for (int t = 0; t < ans_len_size; ++t){
        int aidx = write_num += 1;
        if (aidx >= ans_len_size) {break;}
        const int i = ans_head[aidx];
        const int len = ans_len[aidx];
        int write_offset = ans_write_addr[aidx] + ans_num_offset;
        int_std thread_id = handle_thread_id[i];
        const auto & real_a = real_ans[thread_id];
        const auto & ans = ans_pool[len];
        for (const auto& start_point: ans[i]){
            const string & str = id_str[real_a[start_point]];
            //strcpy(out_buff + write_offset, str.c_str());
            //write_offset += str.length();
            write_offset += str_copy(out_buff + write_offset, str);
            for (unsigned int i = start_point + 1; i < start_point + len; ++i){
                out_buff[write_offset++] = ',';
                const string & str = id_str[real_a[i]];
                write_offset += str_copy(out_buff + write_offset, str);
                //strcpy(out_buff + write_offset, str.c_str());
                //write_offset += str.length();
            }
            out_buff[write_offset++] = '\n';
        }
    }
}


char foo_buff[3000000 * 7 * 11];
void mmap_write_data(char file_name[], const int graph_size){
    int_std total_ans, out_size;
    vector<int_std> ans_len, ans_head, ans_write_addr;
    ans_len.reserve(MAX_ANS_LEN);
    ans_head.reserve(MAX_ANS_LEN);
    ans_write_addr.reserve(MAX_ANS_LEN);
    write_count(graph_size, total_ans, out_size, ans_len, ans_head, ans_write_addr);
    char * out_buff = foo_buff;
    //cout << "file size should be: " << out_size << endl;

    int fd = open(file_name, O_CREAT|O_RDWR|O_TRUNC, 0666);
    int flag = ftruncate(fd, out_size);
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
    out_buff = (char*) mmap(foo_buff, out_size, PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
}

int preprocess(){
    RawGraph.reserve(tD.size());
    id_str.reserve(tD.size());
    id_len.reserve(tD.size());
    id2index.reserve(MAX_ID_NUM);
    id2index.rehash(MAX_ID_NUM);
    index2id.reserve(MAX_ID_NUM);
    int last = -1;
    int count = -1;
    for (int i = 0; i < tD.size(); ++i){
        int_std a = tD[i].first, b = tD[i].second;//, c = C[i];
        if (a != last){
            RawGraph.emplace_back(vector<int_std> ());
            RawGraph.back().reserve(10);
            last = a;
            index2id.emplace_back(a);
            id_str.emplace_back(to_string(a));
            id_len.emplace_back(id_str.back().length());
            id2index[a] = ++count;
        }
        RawGraph[count].emplace_back(b);
    }
    // Rebuild Graph, and remove v which d_out(v) == 0 
    const auto graph_size = RawGraph.size();
    graph.reserve(tD.size());
    in_degree.resize(graph_size);
    out_degree.resize(graph_size);
    first_edge.resize(graph_size);
    last_edge.resize(graph_size);

    handle_thread_id.resize(graph_size);
    rev_graph.resize(graph_size);

    for (int i = 0; i < graph_size; ++i){
        rev_graph[i].reserve(10);
        in_degree[i] = 0;
        out_degree[i] = 0;
    }
    int edge_count = 0;
    for (int_std i = 0; i < graph_size; ++i){
        const auto & rg = RawGraph[i];
        first_edge[i] = edge_count;
        for (const auto & v : rg){
            if (id2index.find(v) == id2index.end()){
                continue;
            }
            const auto target = id2index[v];
            ++edge_count;
            graph.push_back(target);
            ++out_degree[i];
            ++in_degree[target];
            rev_graph[target].emplace_back(i);
        }
        last_edge[i] = edge_count;
        //undo[i] = true;
    }

    for (int i = MIN_LENGTH; i <= MAX_LENGTH; ++i){
        ans_pool[i].resize(graph_size);
    }

    //LOW = vector<int_std> (graph_size, 0);
    //DFN = vector<int_std> (graph_size, 0);
    //visit = vector<int_std> (graph_size, 0);
    //stack = vector<int_std> (graph_size, 0);
    //group_size = vector<int_std> (graph_size, 0);
    //color = vector<int_std> (graph_size, 0);
    //t_index = 0;
    //tot = 0;
    //for (int_std i = 0; i < graph_size; ++i){
    //    if (!DFN[i]){
    //        tarjan(i, 0);
    //    }
    //}

    //new_graph.resize(graph.size());
    //vector<int_std> in(graph_size, 0), outd(graph_size, 0);
    //for (int_std i = 0; i < graph_size; ++i){
    //    const auto & g = graph[i];
    //    if (group_size[color[i]] < 3){
    //        continue;
    //    }
    //    new_graph[i].reserve(graph[i].size());
    //    //new_graph[i].clear();
    //    for (const auto & v : g){
    //        if (color[v] == color[i]){
    //            new_graph[i].emplace_back(v);
    //        }
    //    }
    //}
    //#ifdef DEBUG
    //cout << "Graph size is : " << graph_size << endl;
    //#endif
    //graph = new_graph;
    return graph_size;
}

int main(){
    #ifdef DEBUG
    auto beg_t = clock();
	//char in_file_name[] = "D:/downloads/test_data.txt";
    char in_file_name[] = "/mnt/d/downloads/test_data.txt";
    //char in_file_name[] = "test_data.txt";
    char out_file_name[] = "new_result.txt";
    #else
    char in_file_name[] = "/data/test_data.txt";
    char out_file_name[] = "/projects/student/result.txt";
    #endif
    //FILE * fin = fopen(file_name, "r");
   
    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    quick_input(in_file_name);
    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    //cout << A.size() << endl;
    const int_std graph_size = preprocess();
    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;

    undo.resize(graph_size);
    fill(undo.begin(), undo.end(), 1);
    thread threads[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; ++i){
        threads[i] = thread(run_job, i, graph_size);
    }
    for (int i = 0; i < THREAD_NUM; ++i){
        threads[i].join();
    }

    //sort(ans.begin(), ans.end());   
    //normal_write_data(out_file_name, graph_size);
    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    mmap_write_data(out_file_name, graph_size);
    //_exit(0);
    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    //fclose(fin);
    //cout << time_count * 1. / CLOCKS_PER_SEC << endl;
    return 0;
}


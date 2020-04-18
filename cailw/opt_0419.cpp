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
#pragma comment(linker, "/STACK:4073741824")
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <iostream>
#include <vector>
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

const int MAX_JUMP_SIZE = 512;
const int MIN_LENGTH = 3;
const int MAX_LENGTH = 7;
const int ANS_LENGTH_NUM = MAX_LENGTH - MIN_LENGTH + 1;
const int THREAD_NUM = 4;
const int WRITER_THREAD_NUM = 4;
const int MAX_ANS_NUM = 3000000;
const int MAX_ID_NUM = 280000;
const int MAX_DATA_SIZE = 280000;
const int MAX_NUM_STRING_SIZE = 11;
const int MAX_POOL_SIZE = 20000;

//typedef int int_std;
typedef int int_std;

int_std rev_graph[MAX_DATA_SIZE], rev_first_edge[MAX_DATA_SIZE], rev_last_edge[MAX_DATA_SIZE];
int_std in_degree[MAX_ID_NUM], out_degree[MAX_ID_NUM];

vector<int_std> graph, first_edge, last_edge;

int_std handle_thread_id[MAX_ID_NUM];
int_std real_ans[THREAD_NUM][ANS_LENGTH_NUM][MAX_ANS_NUM * 3], real_ans_size[THREAD_NUM][ANS_LENGTH_NUM];
int_std ans_len[MAX_ANS_NUM], ans_head[MAX_ANS_NUM], ans_write_addr[MAX_ANS_NUM], ans_start_point[MAX_ANS_NUM];
int_std ans_len_size;
char id_str[MAX_ID_NUM][MAX_NUM_STRING_SIZE];
int_std id_len[MAX_ID_NUM];
pair<int_std, int_std> jump_pool[THREAD_NUM][MAX_POOL_SIZE][MAX_JUMP_SIZE];

int_std jump_pool_size[THREAD_NUM][MAX_ID_NUM];

vector<bool> used_pool[THREAD_NUM];
int_std ans_pool[8][MAX_ID_NUM];

inline
void quick_jump(int_std head, 
                int_std v,
                pair<int_std, int_std> (* jump[])[MAX_JUMP_SIZE],
                int_std * jump_size,
                const vector<int_std> & node_list,
                const vector<bool> & used,
                int_std thread_id){
    const int idx = node_list.size() - 1;
    const auto & jp = *jump[v];
    const auto & jp_s = jump_size[v];
    auto & real_a = real_ans[thread_id][idx];
    auto & real_a_len = real_ans_size[thread_id][idx];
    for (int i = 0; i < jp_s; ++i){
        const auto & mid = jp[i];
        if (used[mid.first] == 0 and used[mid.second] == 0){
            ++ans_pool[idx][head];
            for (const auto & u : node_list){
                real_a[real_a_len++] = u;
            }
            real_a[real_a_len++] = mid.first;
            real_a[real_a_len++] = mid.second;
        }
    }
}

void search(int_std head,
            pair<int_std, int_std> (* jump[])[MAX_JUMP_SIZE],
            int_std * jump_size,
            const vector<bool> & jump_update_flag,
            vector<bool> & used,
            int_std thread_id,
            vector<int_std> & first_edge){
    vector<int_std> node_list(MAX_LENGTH);
    node_list.clear();
    node_list.emplace_back(head);
    if (jump_update_flag[head]) quick_jump(head, head, jump, jump_size, node_list, used, thread_id);
    auto itr_u = first_edge[head];
    const auto itr_u_end = last_edge[head];
    for (;itr_u < itr_u_end; ++itr_u) { if (graph[itr_u] > head) break;}
    first_edge[head] = itr_u;
    for (;itr_u < itr_u_end; ++itr_u){
        const auto u = graph[itr_u];
        node_list.emplace_back(u);
        used[u] = 1;
        if (jump_update_flag[u]) quick_jump(head, u, jump, jump_size, node_list, used, thread_id);
        auto itr_v = first_edge[u];
        const auto itr_v_end = last_edge[u];
        for (; itr_v < itr_v_end; ++itr_v){ if (graph[itr_v] > head) break;}
        first_edge[u] = itr_v;
        for (;itr_v < itr_v_end; ++itr_v){
            const auto v = graph[itr_v];
            node_list.emplace_back(v);
            used[v] = 1;
            if (jump_update_flag[v]) quick_jump(head, v, jump, jump_size, node_list, used, thread_id);
			auto itr_k = first_edge[v];
            const auto itr_k_end = last_edge[v];
            for (; itr_k < itr_k_end; ++itr_k){ if (graph[itr_k] > head) break;}
            first_edge[v] = itr_k;
            for (;itr_k < itr_k_end; ++itr_k){
                const auto k = graph[itr_k];
                if (k == u) continue;
                node_list.emplace_back(k);
                used[k] = 1;
                if (jump_update_flag[k]) quick_jump(head, k, jump, jump_size, node_list, used, thread_id);
				auto itr_l = first_edge[k];
                auto itr_l_end = last_edge[k];
				for (; itr_l < itr_l_end; ++itr_l) {
                    if (graph[itr_l] > head) {
                        first_edge[k] = itr_l;
                        break;
                    }
                }
				for (;itr_l < itr_l_end; ++itr_l){
                    const auto l = graph[itr_l];
					if (l != u and l != v and jump_update_flag[l]) {
						node_list.emplace_back(l);
						quick_jump(head, l, jump, jump_size, node_list, used, thread_id);
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
    //used[head] = 0;
}

int init_jump(int_std head, 
            pair<int_std, int_std> (* jump[])[MAX_JUMP_SIZE],
            pair<int_std, int_std> jump_pool[][MAX_JUMP_SIZE],
            int_std * jump_size,
            vector<bool> & jump_update_flag, 
            vector<int_std> & init_node){
    int jump_num = 0;
    int jump_pool_used = 0;
    init_node.clear();
    const auto rev_first_edge_head = rev_first_edge[head];
    for (int itr_u = rev_last_edge[head] - 1; itr_u >= rev_first_edge_head && rev_graph[itr_u] > head; --itr_u){
        const auto u = rev_graph[itr_u];
        const auto rev_first_edge_u = rev_first_edge[u];
        for (int itr_v = rev_last_edge[u] - 1; itr_v >= rev_first_edge_u && rev_graph[itr_v] > head; --itr_v){
            const auto v = rev_graph[itr_v];
            const auto & rev_first_edge_v = rev_first_edge[v];
            for (int itr_mid = rev_last_edge[v] - 1; itr_mid >= rev_first_edge_v && rev_graph[itr_mid] >= head; --itr_mid){
                const auto mid = rev_graph[itr_mid];
                if (mid != u){
                    if (!jump_update_flag[mid]){
                        jump_update_flag[mid] = true;
                        jump_size[mid] = 0;
                        init_node.emplace_back(mid);
                        jump_num+=1;
                        jump[mid] = &jump_pool[jump_pool_used++];
                    }
                    (*jump[mid])[jump_size[mid]++] = make_pair(v, u);
                }
            }
        }
    }
    for (const auto & mid : init_node){
        sort(*jump[mid], *jump[mid] + jump_size[mid]);
    }
    return jump_num;
}

atomic_long handle_num(-1);
void run_job(int_std thread_id, int_std graph_size){
    vector<int_std> init_node;
    vector<int_std> thread_first_edge(first_edge);
    vector<bool> jump_update_flag(graph_size, false);
    int_std * jump_size = jump_pool_size[thread_id];

    pair<int_std, int_std> (* jump[MAX_ID_NUM])[MAX_JUMP_SIZE];

    used_pool[thread_id].resize(graph_size);
    for (int t = 0; t < graph_size; ++t){
        int i = handle_num += 1;
        if (i >= graph_size){break;}
        const int jump_num = init_jump(i, jump, jump_pool[thread_id], jump_size, jump_update_flag, init_node);
        handle_thread_id[i] = thread_id;
        if (jump_num > 0){
            search(i, jump, jump_size, jump_update_flag, used_pool[thread_id], thread_id, thread_first_edge);
            for (const auto & mid : init_node){
                jump_update_flag[mid] = false;
            }
        }
    }
}

const int MAX_BUFF_SIZE = 4096 * 1024;
const int SAFE_BUFF_SIZE = 2048 * 1024;
const int LEAST_DATA_LEFT = 1024;
char out_buff[MAX_BUFF_SIZE];
char in_buff[MAX_BUFF_SIZE];

vector<pair<int_std, int_std>> tD(MAX_DATA_SIZE), rD(MAX_DATA_SIZE);

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
void write_count(const int graph_size, int_std & total_ans, int_std & out_size){
    total_ans = 0;
    out_size = 0;
    for (int len = MIN_LENGTH; len <= MAX_LENGTH; ++len){
        const auto idx = len - MIN_LENGTH;
        const auto & ans = ans_pool[idx];
        int_std start_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
        for (int i = 0; i < graph_size; ++i){
            if (ans[i] == 0) {continue;}
            int_std thread_id = handle_thread_id[i];
            auto & start_point = start_point_pool[thread_id][idx];
            ans_head[ans_len_size] = i;
            ans_write_addr[ans_len_size] = out_size;
            ans_start_point[ans_len_size] = start_point;
            ans_len[ans_len_size++] = len;
            total_ans += ans[i];
            const int_std end_point = start_point + ans[i] * len;
            const auto & real_a = real_ans[thread_id][idx];

            for (; start_point < end_point; start_point += len){
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
                              const int_std ans_num_offset){
    //int_std start_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
    //int_std end_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
    for (int t = 0; t < ans_len_size; ++t){
        int aidx = write_num += 1;
        if (aidx >= ans_len_size) {break;}
        const int head = ans_head[aidx];
        const int len = ans_len[aidx];
        const auto idx = len - MIN_LENGTH;
        int write_offset = ans_write_addr[aidx] + ans_num_offset;
        int_std start_point = ans_start_point[aidx];
        int_std thread_id = handle_thread_id[head];
        const auto & real_a = real_ans[thread_id][idx];
        const int_std end_point = start_point + ans_pool[idx][head] * len;
        for (;start_point < end_point; start_point += len){
            strcpy(out_buff + write_offset, id_str[real_a[start_point]]);
            write_offset += id_len[real_a[start_point]];
            for (unsigned int i = start_point + 1; i < start_point + len; ++i){
                out_buff[write_offset++] = ',';
                strcpy(out_buff + write_offset, id_str[real_a[i]]);
                write_offset += id_len[real_a[i]];
            }
            out_buff[write_offset++] = '\n';
        }
    }
}

void mmap_write_data(char file_name[], const int graph_size){
    int_std total_ans, out_size;
    //ans_len.reserve(MAX_ANS_NUM);
    //ans_head.reserve(MAX_ANS_NUM);
    //ans_write_addr.reserve(MAX_ANS_NUM);
    write_count(graph_size, total_ans, out_size);
    //cout << "file size should be: " << out_size << endl;

    int fd = open(file_name, O_CREAT|O_RDWR|O_TRUNC, 0666);
    int flag = ftruncate(fd, out_size);
    //char * out_buff = (char*) mmap(NULL, out_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
    char * out_buff = (char*) mmap(NULL, out_size, PROT_WRITE, MAP_SHARED, fd, 0);

    string total_ans_str = to_string(total_ans);
    int buff_used = total_ans_str.length();
    strcpy(out_buff, total_ans_str.c_str());
    out_buff[buff_used++] = '\n';
    int ans_num_offset = buff_used;
    #ifdef DEBUG
    cout << "total_ans: " << total_ans << endl;
    #endif

    thread threads[WRITER_THREAD_NUM];
    for (int i = 0; i < WRITER_THREAD_NUM; ++i){
        threads[i] = thread(mmap_multi_thread_writer, i, out_buff, ans_num_offset);
    }
    for (int i = 0; i < WRITER_THREAD_NUM; ++i){
        threads[i].join();
    }
    munmap(out_buff, out_size);
    close(fd);
}

int preprocess(){
    int count = -1;
    int max_node_id = 0;
    for (int i = 0; i < tD.size(); ++i){
        int_std a = tD[i].first, b = tD[i].second;//, c = C[i];
        ++in_degree[b];
        ++out_degree[a];
        if (a > max_node_id){
            max_node_id = a;
        }
        if (b > max_node_id){
            max_node_id = b;
        }   
    }
    const int graph_size = max_node_id + 1;
    first_edge.resize(graph_size);
    last_edge.resize(graph_size);

    int last = -1;
    int edge_count = 0;
    rD.clear();
    for (int i = 0; i < tD.size(); ++i){
        int_std a = tD[i].first, b = tD[i].second;//, c = C[i];
        if (in_degree[a] == 0 or out_degree[b] == 0) continue;
        rD.push_back(make_pair(b, a));
        if (a != last){
            const string & str = to_string(a);
            last = a;
            //id_str[a] = str;
            strcpy(id_str[a], str.c_str());
            id_len[a] = str.length();
            first_edge[a] = edge_count;
        }
        graph.push_back(b);
        last_edge[a] = ++edge_count;
    }

    last = -1;
    sort(rD.begin(), rD.end());
    edge_count = 0;
    for (int i = 0; i < rD.size(); ++i){
        int_std a = rD[i].first, b = rD[i].second;//, c = C[i];
        if (a != last){
            last = a;
            rev_first_edge[a] = edge_count;
        }
        rev_graph[edge_count++] = b;
        rev_last_edge[a] = edge_count;
    }

    #ifdef DEBUG
        cout << "graph_size : " << graph_size << endl;
    #endif 

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
    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    //fclose(fin);
    //cout << time_count * 1. / CLOCKS_PER_SEC << endl;
    return 0;
}



// const auto rev_first_edge_head = rev_first_edge[head];
// //for (int itr_u = rev_last_edge[head] - 1; itr_u >= rev_first_edge_head && rev_graph[itr_u] > head; --itr_u){
// const auto rev_graph_head = rev_graph[head];
// for (int itr_u = rev_graph_size[head] - 1; itr_u >= 0 && rev_graph_head[itr_u] > head; --itr_u){
//     const auto u = rev_graph_head[itr_u];
//     const auto rev_graph_u = rev_graph[u];
//     for (int itr_v = rev_graph_size[u] - 1; itr_v >= 0 && rev_graph_u[itr_v] > head; --itr_v){
//         const auto v = rev_graph_u[itr_v];
//         const auto rev_graph_v = rev_graph[v];
//         for (int itr_mid = rev_graph_size[v] - 1; itr_mid >= 0 && rev_graph_v[itr_mid] >= head; --itr_mid){
//             const auto mid = rev_graph_v[itr_mid];
//             if (mid != u){
//                 if (!jump_update_flag[mid]){
//                     jump_update_flag[mid] = true;
//                     jump_size[mid] = 0;
//                     init_node.emplace_back(mid);
//                     jump_num+=1;
//                 }
//                 jump[mid][jump_size[mid]++] = make_pair(v, u);
//             }
//         }
//     }
// }

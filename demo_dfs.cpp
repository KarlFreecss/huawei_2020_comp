/*
TODO:
    0. still need know the cost of (input, preprocess, search, output) of online data;
    1. re-id
    2. more good align of graph data;
        2.0 Sort operator need be carefully, [(T *)addr_begin, (T *)addr_end];
        2.1 Current edge strategy is (receiver, amount);
        2.2 Maybe need continue consider (receiver, amount, first_edge[receiver])
    3. answer store 
*/
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <thread>
#include <atomic>
#include <fstream>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ctime>

#define TEST

#ifdef TEST
#define INPUT_FILE_NAME "test_data.txt"
#define OUTPUT_FILE_NAME "result.txt"
#endif

#ifdef TEST
#define REQUIRE_DEBUG_INFO
#endif

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define L1_CACHE_LINE_SIZE  (64)
#define L1_CACHE_SIZE       (64 * (1 << 10))
#define L2_CACHE_LINE_SIZE  (64)
#define L2_CACHE_SIZE       (512 * (1 << 10))
#define L3_CACHE_LINE_SIZE  (128)
#define L3_CACHE_SIZE       (1 * (1 << 20))

#define CACHE_LINE_SIZE L1_CACHE_LINE_SIZE

using namespace std;

typedef int int_std;
typedef unsigned int uint_std;
typedef unsigned char uint_byte;

const int MAX_DATA_RECORD_SIZE  = 2000000;
const int MAX_ANS_NUM           = 5000000; //TODO : 20000000
const int MIN_PATH_LENGTH       = 3;
const int MAX_PATH_LENGTH       = 7;
const int MAX_ID_NUM            = MAX_DATA_RECORD_SIZE * 2; // Every record has two relatived account
const int PATH_LENGTH_NUM       = MAX_PATH_LENGTH - MIN_PATH_LENGTH + 1;

const int THREAD_NUM            = 4;
const int WRITER_THREAD_NUM     = 4;
const int MAX_ID_STRING_LENGTH = 12; //log(2 ** 32) + 1
const int ID_HASH_TABLE_SIZE    = 27579263; // MAX_ID_NUM * 2 * 7, bigger is better;

/////////////////////////////////////////////////
////////////////////// Hash Code ////////////////
/////////////////////////////////////////////////
/*
    Here, we face a trade-off about set all byte of id_hash_table to [0xff],
    or, use another table to record whether it be used.
*/

int_std real_ans[THREAD_NUM][MAX_ANS_NUM * MAX_PATH_LENGTH];
uint_std id_hash_table_key[ID_HASH_TABLE_SIZE];
uint_std id_hash_table_value[ID_HASH_TABLE_SIZE];
uint_byte id_hash_table_flag[ID_HASH_TABLE_SIZE];

/*
    The purpose of hash is mapping id to unique rank (or idx).
    And maybe check id whether this id is appeared.
    The id num is smaller than ID_HASH_TABLE_SIZE, so, it don't have any size check.
*/
inline
int_std get_hash_idx(int_std id){
    int_std idx = id % ID_HASH_TABLE_SIZE;
    while (id_hash_table_flag[idx] != 0 && id_hash_table_key[idx] != id) {
        ++idx;
        if (idx >= ID_HASH_TABLE_SIZE) {idx = 0;}
    }
    return idx;
}

inline
int_std hash_insert(int_std id){
    int_std idx = get_hash_idx(id);
    if (id_hash_table_flag[idx] != 0) return -1;
    id_hash_table_flag[idx] = 1;
    id_hash_table_key[idx] = id;
    return idx;
}

inline
int_std hash_mapping(int_std id, int_std value){
    int_std idx = get_hash_idx(id);
    id_hash_table_flag[idx] = 1;
    id_hash_table_key[idx] = id;
    id_hash_table_value[idx] = value;
    return value;
}

inline
int_std hash_query(int_std id){
    int_std idx = get_hash_idx(id);
    if (id_hash_table_flag[idx] == 0) return -1;
    return id_hash_table_value[idx];
}

/////////////////////////////////////////////////
///////////////// Graph Access //////////////////
/////////////////////////////////////////////////

struct Data{
    int a, b, c;
};

const int SINGLE_CACHE_LINE_EDGE_NUM = CACHE_LINE_SIZE / (sizeof(int_std));
const int MAX_GRAPH_SIZE = ((MAX_ID_NUM - 1) / SINGLE_CACHE_LINE_EDGE_NUM + 1) * SINGLE_CACHE_LINE_EDGE_NUM + MAX_DATA_RECORD_SIZE * 2;

int_std graph[MAX_GRAPH_SIZE];
int first_edge[MAX_ID_NUM];
int end_edge[MAX_ID_NUM];

inline
void graph_node_edge_init(int_std * graph, int * first_edge, int * nums, int valid_graph_size){
    graph[0] = 2;
    graph[1] = 2;
    first_edge[0] = 0;
    for (int idx = 1; idx < valid_graph_size; ++idx){
    // In fact, there should be not num equal to zero.
        int num = nums[idx - 1];
        //if (unlikely(num == 0)){
        //    first_edge[idx] = first_edge[idx - 1];
        //} else {
        {
            int store_size = ((2 * num + 1) / SINGLE_CACHE_LINE_EDGE_NUM + 1) * SINGLE_CACHE_LINE_EDGE_NUM;
            first_edge[idx] = first_edge[idx - 1] + store_size;
            graph[first_edge[idx]] = first_edge[idx] + 2;
            graph[first_edge[idx] + 1] = first_edge[idx] + 2;
        }
        int start_point = first_edge[idx];
    }
}

inline
void graph_node_edge_add(int_std * graph, int * first_edge, Data * tD, int tD_size){
    for (int i = 0; i < tD_size; ++i){
        const auto & d = tD[i];
        int start_point = first_edge[d.a];
        int & end_idx = graph[start_point];
        graph[end_idx] = d.b;
        graph[end_idx + 1] = d.c;
        end_idx += 2;
        int idx = d.a;
    }
}

struct EdgeData{
    int_std receiver;
    int_std amount;

    bool operator < (const EdgeData & A) const {
        return receiver < A.receiver;
    }
};

inline 
void graph_node_edge_sort(int_std * graph, int * first_edge, int valid_graph_size){
    for (int i = 0; i < valid_graph_size; ++i){
        int start_point = first_edge[i];
        sort((EdgeData *)(graph + start_point + 2), (EdgeData *)(graph + graph[start_point]));
    }
}

Data tD[MAX_DATA_RECORD_SIZE];
int tD_size;

inline
int_std get_num(char buff[], int_std & used_len){
    while (buff[used_len] < '0' or buff[used_len] > '9') ++used_len;
    const int begin_used_len = used_len;
    int_std ret = buff[used_len++] - '0';
    while (buff[used_len] >= '0' && buff[used_len] <= '9') {
        ret = ret * 10 + buff[used_len++] - '0';
    }
    //if (id_len[ret] == 0){
    //    const int len = used_len - begin_used_len;
    //    memcpy(id_str[ret] + 1, buff + begin_used_len, len);
    //    id_len[ret] = len;
    //    id_str[ret][0] = len;
    //}
    return ret;
}


/////////////////////////////////////////////////
///////////////// IO Operation //////////////////
/////////////////////////////////////////////////

const int MAX_BUFF_SIZE = 4096 * 1024;
const int SAFE_BUFF_SIZE = 2048 * 1024;
const int LEAST_DATA_LEFT = 1024;
char in_buff[MAX_BUFF_SIZE];

int_std ids[MAX_ID_NUM];
int_std ids_size;
#define PUSH_BACK(A, A_size, x) A[A_size++] = (x)

void quick_input(char * file_name){
    FILE * fin = fopen(file_name, "r");
    int_std is_finished = false;
    int_std used_len = 0;
    int_std left_data_size = 0;
    while (false == is_finished){
        int_std read_size = fread(in_buff + left_data_size, 1, SAFE_BUFF_SIZE, fin);
        left_data_size += read_size;
        if (read_size < SAFE_BUFF_SIZE) is_finished = true;
        while (left_data_size - used_len > LEAST_DATA_LEFT){
            const int a = get_num(in_buff, used_len);
            const int b = get_num(in_buff, used_len);
            const int c = get_num(in_buff, used_len);
            //if (a == b){continue;}
            PUSH_BACK(tD, tD_size, ((Data){a,b,c}));
            if (hash_insert(a) >= 0) PUSH_BACK(ids, ids_size, a);
            if (hash_insert(b) >= 0) PUSH_BACK(ids, ids_size, b);
        }
        left_data_size = left_data_size - used_len;
        memcpy(in_buff, in_buff + used_len, left_data_size);
        used_len = 0;
    }
    while (left_data_size - used_len > 3){
        const int a = get_num(in_buff, used_len);
        const int b = get_num(in_buff, used_len);
        const int c = get_num(in_buff, used_len);
        //if (a == b){continue;}
        PUSH_BACK(tD, tD_size, ((Data){a,b,c}));
        if (hash_insert(a) >= 0) PUSH_BACK(ids, ids_size, a);
        if (hash_insert(b) >= 0) PUSH_BACK(ids, ids_size, b);
    }
    sort(ids, ids + ids_size);
    fclose(fin);
}

//void mmap_input(char * file_name){
//    int fd = open(file_name, O_RDONLY);
//    long file_len = lseek(fd, 0, SEEK_END);
//    char * in_buff = (char*) mmap(NULL, file_len, PROT_READ, MAP_PRIVATE, fd, 0);
//    int used_len = 0;
//    tD.clear();
//    while (file_len - used_len > 3){
//        const int a = get_num(in_buff, used_len);
//        const int b = get_num(in_buff, used_len);
//        const int c = get_num(in_buff, used_len);
//        PUSH_BACK(tD, tD_size, ((Data){a,b,c}));
//        if (hash_insert(a) >= 0) PUSH_BACK(ids, ids_size, a);
//        if (hash_insert(b) >= 0) PUSH_BACK(ids, ids_size, b);
//    }
//    close(fd);
//}

/////////////////////////////////////////////////
////////////////// Preprocess ///////////////////
/////////////////////////////////////////////////

int in_degree[MAX_ID_NUM];
int out_degree[MAX_ID_NUM];
char id_str[MAX_ID_NUM][((MAX_ID_STRING_LENGTH) / 8 + 1) * 8];
char id_len[MAX_ID_NUM];

inline 
void reid(){
    for (int i = 0; i < ids_size; ++i){
        hash_mapping(ids[i], i);
        const string str = to_string(ids[i]);
        id_len[i] = str.length();
        id_str[i][0] = str.length();
        memcpy(id_str[i] + 1, str.c_str(), id_len[i]);
    }
    for (int i = 0; i < tD_size; ++i){
        auto & d = tD[i];
        d.a = hash_query(d.a);
        d.b = hash_query(d.b);
        out_degree[d.a] += 1;
        in_degree[d.b] += 1;
    }
}

void preprocess(){
    reid();
    graph_node_edge_init(graph, first_edge, out_degree, ids_size);
    graph_node_edge_add(graph, first_edge, tD, tD_size);
    graph_node_edge_sort(graph, first_edge, ids_size);
}

/////////////////////////////////////////////////
/////////////////////////////////////////////////

//ll means long long type!!!
#define amount_valid(X,Y) (((X)<=5ll*(Y))&&((Y)<=3ll*(X)))

vector<int_std> node_list;
vector<pair<int, vector<int_std>>> ans;
vector<bool> used;
int total_ans = 0;
void dfs(int_std head, int_std cur, int_std depth, int_std first_edge_amount, int_std cur_edge_amount){
    int store_point = first_edge[cur];
    int start_point = graph[store_point+1];
    int end_point = graph[store_point];
    for (;start_point < end_point; start_point += 2){
        int u = graph[start_point];
        if (u == head){
            if (depth >= 3){
                int amount = graph[start_point+1];
                if (amount_valid(cur_edge_amount, amount) && amount_valid(amount, first_edge_amount)){
                    ans.push_back(make_pair(depth, node_list));
                    total_ans++;
                    if (head == 577 and cur == 84992) {
                        cout << head << ' ' << cur << ' ' << endl;
                        cout << cur_edge_amount << ' ' << amount << ' ' << first_edge_amount << endl;
                    }
                    if ((total_ans % 100000) == 0){
                        cout << total_ans << endl;
                    }
                }
            }
            continue;
        }
        if (depth <= 6 && head < u && used[u] == false){
            int amount = graph[start_point+1];
            if (amount_valid(cur_edge_amount, amount)){
                used[u] = 1;
                node_list.push_back(u);
                dfs(head, u, depth+1, first_edge_amount, amount);
                node_list.pop_back();
                used[u] = 0;
            }
        }
    }
}

void run_job(){
    used.resize(ids_size);
    for (int head = 0; head < ids_size; ++head){
        int store_point = first_edge[head];
        int start_point = graph[store_point + 1];
        int end_point = graph[store_point];
        node_list.push_back(head);
        used[head] = 1;
        for (;start_point < end_point; start_point += 2){
            int u = graph[start_point];
            if (u < head) continue;
            int amount = graph[start_point+1];
            node_list.push_back(u);
            used[u] = 1;
            dfs(head, u, 2, amount, amount);
            used[u] = 0;
            node_list.pop_back();
        }
        used[head] = 0;
        node_list.pop_back();
    }
    ofstream fout("dfs_result.txt");
    fout << ans.size() << endl;
    sort(ans.begin(), ans.end());
    for (const auto & d : ans){
        const auto & a = d.second;
        fout << (id_str[a[0]] + 1);
        for (int i = 1; i < a.size(); ++i){
            fout << "," << (id_str[a[i]] + 1);
        }
        fout << endl;
    }
}

void memory_report(){
#define INFO_M(x) cout << #x << " is: " << x / (1000 * 1000) << "M" << endl
    INFO_M(MAX_ID_NUM);
    INFO_M(MAX_DATA_RECORD_SIZE);
    INFO_M(MAX_ANS_NUM);
    cout << "Graph memory size is: " << (sizeof(graph) >> 20) << "MB" << endl;
    cout << "real_ans memory size is: " << (sizeof(real_ans) >> 20) << "MB" << endl;
}

int main(){
    #ifdef REQUIRE_DEBUG_INFO
    memory_report();
    #endif

    // Delete compile warning!
    char input_file_name[] = INPUT_FILE_NAME;
    quick_input(input_file_name);

    #ifdef REQUIRE_DEBUG_INFO
    cout << "Finished input! The unique id number is: " << ids_size << endl;
    #endif

    preprocess();
    
    run_job();

    return 0;
}


//typedef int int_std;
//
//int_std rev_graph[MAX_DATA_SIZE + MAX_ID_NUM * 2], rev_first_last[MAX_DATA_SIZE], tmp_rev_end[MAX_DATA_SIZE];
//int_std graph[MAX_DATA_SIZE], first_edge[MAX_ID_NUM], last_edge[MAX_ID_NUM];
//int_std in_degree[MAX_ID_NUM], out_degree[MAX_ID_NUM];
//
//int_std handle_thread_id[MAX_ID_NUM];
//int_std real_ans[THREAD_NUM][ANS_LENGTH_NUM][MAX_ANS_NUM * 7];
//int_std real_ans_size[THREAD_NUM][ANS_LENGTH_NUM];
//int_std ans_len[MAX_ANS_NUM];
//int_std ans_head[MAX_ANS_NUM];
//int_std ans_write_addr[MAX_ANS_NUM];
//int_std ans_start_point[MAX_ANS_NUM];
//int_std ans_len_size;
//char id_str[MAX_ID_NUM][MAX_NUM_STRING_SIZE + 1];
//char id_len[MAX_ID_NUM];
//
//vector<bool> used_pool[THREAD_NUM];
//int_std ans_pool[ANS_LENGTH_NUM][MAX_ID_NUM];
//
////inline
//void quick_jump(int_std head, 
//                int_std v,
//                vector<vector<pair<int, int>>> & jump,
//                const int_std * node_list,
//                const vector<bool> & used,
//                int_std thread_id){
//    const int idx = node_list[0] + (head != v);
//    const auto & jp = jump[v];
//    auto & real_a = real_ans[thread_id][idx];
//    auto & real_a_len = real_ans_size[thread_id][idx];
//    for (const auto & mid : jp){
//        if (used[mid.first] == false and used[mid.second] == false){
//            ++ans_pool[idx][head];
//            const int node_list_len = node_list[0];
//            real_a[real_a_len++] = head;
//            for (int i = 1; i <= node_list_len; ++i){
//                real_a[real_a_len++] = node_list[i];
//            }
//            if (v != head){
//                real_a[real_a_len++] = v;
//            }
//            real_a[real_a_len++] = mid.first;
//            real_a[real_a_len++] = mid.second;
//        }
//    }
//}
//
//void search(int_std head,
//            vector<vector<pair<int, int>>> & jump,
//            const vector<bool> & jump_update_flag,
//            vector<bool> & used,
//            int_std thread_id,
//            int_std * first_edge){
//    int_std node_list[4];
//    int_std & node_list_len = node_list[0];
//    node_list_len = 0;
//    if (jump_update_flag[head]) quick_jump(head, head, jump, node_list, used, thread_id);
//    //used[head]  = 1;
//    auto itr_u = first_edge[head];
//    const auto itr_u_end = last_edge[head];
//    for (;itr_u < itr_u_end; ++itr_u) { if (graph[itr_u] > head) break;}
//    first_edge[head] = itr_u;
//    for (;itr_u < itr_u_end; ++itr_u){
//        const auto u = graph[itr_u];
//        used[u] = 1;
//        if (jump_update_flag[u]) quick_jump(head, u, jump, node_list, used, thread_id);
//        node_list[++node_list_len] = u;
//        auto itr_v = first_edge[u];
//        const auto itr_v_end = last_edge[u];
//        for (; itr_v < itr_v_end; ++itr_v){ if (graph[itr_v] > head) break;}
//        first_edge[u] = itr_v;
//        for (;itr_v < itr_v_end; ++itr_v){
//            const auto v = graph[itr_v];
//            used[v] = 1;
//            //quick_jump(head, v, jump, node_list, used);
//            if (jump_update_flag[v]) quick_jump(head, v, jump, node_list, used, thread_id);
//            node_list[++node_list_len] = v;
//			auto itr_k = first_edge[v];
//            const auto itr_k_end = last_edge[v];
//            for (; itr_k < itr_k_end; ++itr_k){ if (graph[itr_k] > head) break;}
//            first_edge[v] = itr_k;
//            for (;itr_k < itr_k_end; ++itr_k){
//                const auto k = graph[itr_k];
//                if (k == u) continue;
//                used[k] = 1;
//                //quick_jump(head, k, jump, node_list, used);
//                if (jump_update_flag[k]) quick_jump(head, k, jump, node_list, used, thread_id);
//                node_list[++node_list_len] = k;
//				auto itr_l = first_edge[k];
//                const auto itr_l_end = last_edge[k];
//				for (; itr_l < itr_l_end; ++itr_l) {
//                    if (graph[itr_l] > head) {
//                        first_edge[k] = itr_l;
//                        break;
//                    }
//                }
//				for (;itr_l < itr_l_end; ++itr_l){
//                    const auto l = graph[itr_l];
//					if (unlikely(jump_update_flag[l] and l != u and l != v)) {
//						quick_jump(head, l, jump, node_list, used, thread_id);
//					}
//                }
//                used[k] = 0;
//                node_list_len = 2;
//            }
//            used[v] = 0;
//            node_list_len = 1;
//        }
//        used[u] = 0;
//        node_list_len = 0;
//    }
//    //used[head] = 0;
//}
//
//int init_jump(int_std head, 
//            vector<vector<pair<int, int>>> & jump, 
//            vector<bool> & jump_update_flag, 
//            vector<int_std> & init_node){
//    int jump_num = 0;
//    init_node.clear();
//    for (int itr_u = rev_first_last[head]; rev_graph[itr_u] > head; --itr_u){
//        const auto u = rev_graph[itr_u];
//        for (int itr_v = rev_first_last[u]; rev_graph[itr_v] > head; --itr_v){
//            const auto v = rev_graph[itr_v];
//            for (int itr_mid = rev_first_last[v]; rev_graph[itr_mid] >= head; --itr_mid){
//                const auto mid = rev_graph[itr_mid];
//                if (mid != u){
//                    if (!jump_update_flag[mid]){
//                        jump_update_flag[mid] = true;
//                        jump[mid].clear();
//                        init_node.emplace_back(mid);
//                        jump_num+=1;
//                    }
//                    jump[mid].emplace_back((pair<int, int>){v, u});
//                }
//            }
//        }
//    }
//    for (const auto & mid : init_node){
//        if (jump[mid].size() > 1) {sort(jump[mid].begin(), jump[mid].end());}
//    }
//    return jump_num;
//}
//
//atomic_long handle_num(-1);
//void run_job(int_std thread_id, int_std graph_size){
//    vector<int_std> init_node;
//    int_std thread_first_edge[graph_size];
//    memcpy(thread_first_edge, first_edge, graph_size * sizeof(int_std));
//    vector<bool> jump_update_flag(graph_size, false);
//    vector<vector<pair<int, int>>> jump_pool(graph_size);
//
//    used_pool[thread_id].resize(graph_size);
//    for (int t = 0; t < graph_size; ++t){
//        int i = handle_num += 1;
//        if (i >= graph_size){break;}
//        const int jump_num = init_jump(i, jump_pool, jump_update_flag, init_node);
//        handle_thread_id[i] = thread_id;
//        if (jump_num > 0){
//            search(i, jump_pool, jump_update_flag, used_pool[thread_id], thread_id, thread_first_edge);
//            for (const auto & mid : init_node){
//                jump_update_flag[mid] = false;
//            }
//        }
//    }
//}
//
//
//
//
//inline
//void write_count(const int graph_size, int_std & total_ans, int_std & out_size){
//    total_ans = 0;
//    out_size = 0;
//    for (int len_t = MIN_LENGTH; len_t < MAX_LENGTH; ++len_t){
//        const int len = len_t;
//        const auto idx = len - MIN_LENGTH;
//        const auto & ans = ans_pool[idx];
//        int_std start_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
//        for (int i = 0; i < graph_size; ++i){
//            if (ans[i] == 0) {continue;}
//            int_std thread_id = handle_thread_id[i];
//            auto & start_point = start_point_pool[thread_id][idx];
//            ans_head[ans_len_size] = i;
//            ans_write_addr[ans_len_size] = out_size;
//            ans_start_point[ans_len_size] = start_point;
//            ans_len[ans_len_size++] = len;
//            total_ans += ans[i];
//            const int_std end_point = start_point + ans[i] * len;
//            const auto & real_a = real_ans[thread_id][idx];
//
//            for (; start_point < end_point; start_point += len){
//                out_size += id_len[real_a[start_point]] + len;
//                out_size += id_len[real_a[start_point + 1]];
//                out_size += id_len[real_a[start_point + 2]];
//                const int tmp_end_point = start_point + len;
//                for (unsigned int i = start_point + 3; i < tmp_end_point; ++i){
//                    out_size += id_len[real_a[i]];
//                }
//                //++out_size;
//            }
//        }
//    }
//
//    //{
//    //    const int len = 6;
//    //    const auto idx = len - MIN_LENGTH;
//    //    const auto & ans = ans_pool[idx];
//    //    int_std start_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
//    //    for (int i = 0; i < graph_size; ++i){
//    //        if (ans[i] == 0) {continue;}
//    //        int_std thread_id = handle_thread_id[i];
//    //        auto & start_point = start_point_pool[thread_id][idx];
//    //        ans_head[ans_len_size] = i;
//    //        ans_write_addr[ans_len_size] = out_size;
//    //        ans_start_point[ans_len_size] = start_point;
//    //        ans_len[ans_len_size++] = len;
//    //        total_ans += ans[i];
//    //        const int_std end_point = start_point + ans[i] * len;
//    //        out_size += ans[i] * len;
//    //        const auto & real_a = real_ans[thread_id][idx];
//
//    //        for (; start_point < end_point; start_point += len){
//    //            out_size += id_len[real_a[start_point]];
//    //            out_size += id_len[real_a[start_point + 1]];
//    //            out_size += id_len[real_a[start_point + 2]];
//    //            out_size += id_len[real_a[start_point + 3]];
//    //            out_size += id_len[real_a[start_point + 4]];
//    //            out_size += id_len[real_a[start_point + 5]];
//    //            //++out_size;
//    //        }
//    //    }
//    //}
//
//    {
//        const int len = MAX_LENGTH;
//        const auto idx = len - MIN_LENGTH;
//        const auto & ans = ans_pool[idx];
//        int_std start_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
//        for (int i = 0; i < graph_size; ++i){
//            if (ans[i] == 0) {continue;}
//            int_std thread_id = handle_thread_id[i];
//            auto & start_point = start_point_pool[thread_id][idx];
//            ans_head[ans_len_size] = i;
//            ans_write_addr[ans_len_size] = out_size;
//            ans_start_point[ans_len_size] = start_point;
//            ans_len[ans_len_size++] = len;
//            total_ans += ans[i];
//            const int_std end_point = start_point + ans[i] * len;
//            out_size += ans[i] * len;
//            const auto & real_a = real_ans[thread_id][idx];
//            for (; start_point < end_point; start_point += len){
//                out_size += id_len[real_a[start_point + 0]];
//                out_size += id_len[real_a[start_point + 1]];
//                out_size += id_len[real_a[start_point + 2]];
//                out_size += id_len[real_a[start_point + 3]];
//                out_size += id_len[real_a[start_point + 4]];
//                out_size += id_len[real_a[start_point + 5]];
//                out_size += id_len[real_a[start_point + 6]];
//                //++out_size;
//            }
//        }
//    }
//    out_size += to_string(total_ans).length() + 1;
//}
//
//inline int_std str_copy(char *dst, const string &str){
//    const char * raw = str.c_str();
//    const int_std sz = str.size();
//    memcpy(dst, raw, sz);
//    return sz;
//}
//
//atomic_long write_num(-1);
//void mmap_multi_thread_writer(const int_std writer_id,
//                              char * out_buff){
//    //int_std start_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
//    //int_std end_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
//    auto beg_t = clock();
//    for (int t = 0; t < ans_len_size; ++t){
//        int aidx = write_num += 1;
//        if (aidx >= ans_len_size) {break;}
//        const int head = ans_head[aidx];
//        const int len = ans_len[aidx];
//        const auto idx = len - MIN_LENGTH;
//        int write_offset = ans_write_addr[aidx];
//        int_std start_point = ans_start_point[aidx];
//        int_std thread_id = handle_thread_id[head];
//        const auto & real_a = real_ans[thread_id][idx];
//        const int_std end_point = start_point + ans_pool[idx][head] * len;
//        for (;start_point < end_point; start_point += len){
//            const int a = real_a[start_point];
//            memcpy(out_buff + write_offset, id_str[a] + 1, id_str[a][0]);
//            write_offset += id_str[a][0];
//            for (unsigned int i = start_point + 1; i < start_point + len; ++i){
//                out_buff[write_offset++] = ',';
//                const int a = real_a[i];
//                memcpy(out_buff + write_offset, id_str[a] + 1, id_str[a][0]);
//                write_offset += id_str[a][0];
//            }
//            out_buff[write_offset++] = '\n';
//        }
//    }
//}
//
//void mmap_write_data(char file_name[], const int graph_size){
//    #ifdef DEBUG
//    auto beg_t = clock();
//    #endif
//    int_std total_ans, out_size;
//    //ans_len.reserve(MAX_ANS_NUM);
//    //ans_head.reserve(MAX_ANS_NUM);
//    //ans_write_addr.reserve(MAX_ANS_NUM);
//    write_count(graph_size, total_ans, out_size);
//    //cout << "file size should be: " << out_size << endl;
//
//    int fd = open(file_name, O_CREAT|O_RDWR|O_TRUNC, 0666);
//    int flag = ftruncate(fd, out_size);
//    //char * out_buff = (char*) mmap(NULL, out_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
//    //char * out_buff = (char*) mmap(NULL, out_size, PROT_WRITE, MAP_SHARED, fd, 0);
//    char * out_buff = (char*) mmap(NULL, out_size, PROT_WRITE, MAP_SHARED | MAP_POPULATE | MAP_NONBLOCK, fd, 0);
//
//    string total_ans_str = to_string(total_ans);
//    int buff_used = total_ans_str.length();
//    strcpy(out_buff, total_ans_str.c_str());
//    out_buff[buff_used++] = '\n';
//    int ans_num_offset = buff_used;
//
//    #ifdef DEBUG
//    cout << "mmap write count time cost : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//    cout << "total_ans: " << total_ans << endl;
//    #endif
//
//    thread threads[WRITER_THREAD_NUM];
//    for (int i = 0; i < WRITER_THREAD_NUM-1; ++i){
//        threads[i] = thread(mmap_multi_thread_writer, i, out_buff + ans_num_offset);
//    }
//    mmap_multi_thread_writer(WRITER_THREAD_NUM - 1, out_buff + ans_num_offset);
//    //mmap_multi_thread_writer(WRITER_THREAD_NUM - 1, out_buff, ans_num_offset);
//    for (int i = 0; i < WRITER_THREAD_NUM - 1; ++i){
//        threads[i].join();
//    }
//
//    #ifdef DEBUG
//    cout << "mmap write time cost       : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//    #endif
//    //munmap(out_buff, out_size);
//    //_exit(0);
//    close(fd);
//}
//
//int preprocess(){
//    int count = -1;
//    int max_node_id = 0;
//    for (int i = 0; i < tD.size(); ++i){
//        int_std a = tD[i].first, b = tD[i].second;//, c = C[i];
//        ++in_degree[b];
//        ++out_degree[a];
//        if (a > max_node_id){
//            max_node_id = a;
//        }
//        if (b > max_node_id){
//            max_node_id = b;
//        }   
//    }
//    const int graph_size = max_node_id + 1;
//    rev_first_last[0] = 1;
//    tmp_rev_end[0] = 1;
//    rev_graph[0] = -1;
//    for (int i = 1; i < graph_size; ++i){
//        if (in_degree[i-1] != 0){
//            rev_first_last[i] = rev_first_last[i-1] + in_degree[i-1] + 1;
//            rev_graph[rev_first_last[i]-1] = -1;
//        } else {
//            rev_first_last[i] = rev_first_last[i-1];
//        }
//        tmp_rev_end[i] = rev_first_last[i];
//        first_edge[i] = first_edge[i-1] + out_degree[i-1];
//        last_edge[i] = first_edge[i];
//    }
//
//    int last = -1;
//    int edge_count = 0;
//    for (int i = 0; i < tD.size(); ++i){
//        int_std a = tD[i].first, b = tD[i].second;//, c = C[i];
//        if (in_degree[a] == 0 or out_degree[b] == 0) continue;
//        graph[last_edge[a]++] = b;
//        rev_graph[rev_first_last[b]++] = a;
//    }
//
//    for (int i = 0; i < graph_size; ++i){
//        if (last_edge[i] - first_edge[i] > 1){
//            sort(graph + first_edge[i], graph + last_edge[i]);
//        }
//        if (rev_first_last[i] - tmp_rev_end[i] > 1){
//            sort(rev_graph + tmp_rev_end[i], rev_graph + rev_first_last[i]);
//        }
//        rev_first_last[i] -= 1;
//    }
//    //for (int i = 0; i < 100; ++i) {
//    //    cout << rev_graph[i] << ' ';
//    //} cout << endl;
//    //for (int k = 0; k < 10; ++k){
//    //    cout << rev_first_last[k] << ' ' << tmp_rev_end[k] << endl;
//    //} cout << endl;
//
//    #ifdef DEBUG
//        cout << "graph_size : " << graph_size << endl;
//    #endif 
//
//    return graph_size;
//}
//
//int main(){
//    #ifdef TEST
//	//char in_file_name[] = "D:/downloads/test_data.txt";
//    char in_file_name[] = "/mnt/d/downloads/test_data.txt";
//    //char in_file_name[] = "/data/test_data.txt";
//    //char in_file_name[] = "test_data.txt";
//    char out_file_name[] = "new_result.txt";
//    #else
//    char in_file_name[] = "/data/test_data.txt";
//    char out_file_name[] = "/projects/student/result.txt";
//    #endif
//
//    #ifdef DEBUG 
//    auto beg_t = clock();
//    #endif
//    //FILE * fin = fopen(file_name, "r");
//   
//    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//    //quick_input(in_file_name);
//    mmap_input(in_file_name);
//    #ifdef DEBUG
//    cout << "input time cost            : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//    #endif
//    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//    //cout << A.size() << endl;
//    const int_std graph_size = preprocess();
//    #ifdef DEBUG
//    cout << "preprocess time cost       : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//    #endif
//    //cout << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//
//    thread threads[THREAD_NUM - 1];
//    for (int i = 0; i < THREAD_NUM - 1; ++i){
//        threads[i] = thread(run_job, i, graph_size);
//    }
//    run_job(THREAD_NUM - 1, graph_size);
//    for (int i = 0; i < THREAD_NUM - 1; ++i){
//        threads[i].join();
//    }
//
//
//    //sort(ans.begin(), ans.end());   
//    //normal_write_data(out_file_name, graph_size);
//    #ifdef DEBUG
//    cout << "after search time cost     : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//    #endif
//    //exit(0);
//    mmap_write_data(out_file_name, graph_size);
//    #ifdef DEBUG
//    cout << "Total time cost            : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
//    #endif
//    //fclose(fin);
//    //cout << time_count * 1. / CLOCKS_PER_SEC << endl;
//    return 0;
//}
//

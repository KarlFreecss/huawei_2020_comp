/*
TODO:
    0. still need know the cost of (input, preprocess, search, output) of online data;
    1. re-id (maybe need test : tarjan and re-id)
    2. more good align of graph data;
        2.0 Sort operator need be carefully, [(T *)addr_begin, (T *)addr_end];
      ->2.1 Current edge strategy is (receiver, amount);
            TODO: need copy graph every thread.
        2.2 Maybe need continue consider (receiver, amount, first_edge[receiver])
            This align will add 1/3 cache missing afford and reduce first_edge access cache missing;
            It is obviously, in search function first 3 for loop can use 2.2 strategy 
            but fourth for loop use 2.1 is good enough.
        2.3 every cache line blcok stores start_point, end_point, min_amount, max_amount
    3. answer store, I think just like before is ok, but need do some adjustion,
        because avg(3 + 4 + 5 + 6 + 7) is 5 less than 7.
    4. physical memory is less than virtual memory;
    5. maybe we can use segment tree, to quick judge whether the amount is suitable.
        But this method will incur extra access cost.
        So it would be a trade-off about how width range the leave node of segment tree is.
        When the id of head increasing, the meaning size would be less and 
            segment tree would be expensive.
        So, we can try every cache line block record the max-min value;
        [r_0, a_0, r_1, a_1, ..., r_6, a_6, min, max]
        This would change our itr function, the detail is that:
            for code in each block.
        The more easy way is, record max-min info in node_info, easy, quick but useful.
        SLOWLY. I am sure, this will make program run slowlier.
    6. data write need consider false share.

*/

#include <cstdio>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <thread>
#include <atomic>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ctime>
#include <chrono>

//#define TEST

#ifdef TEST
//#define INPUT_FILE_NAME "test_data.txt"
//#define OUTPUT_FILE_NAME "result.txt"
#define INPUT_FILE_NAME "/data/test_data.txt"
#define OUTPUT_FILE_NAME "/projects/student/result.txt"
#else
#define INPUT_FILE_NAME "/data/test_data.txt"
#define OUTPUT_FILE_NAME "/projects/student/result.txt"
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

#define CACHE_LINE_SIZE L3_CACHE_LINE_SIZE
#define ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

using namespace std;

typedef int int_std;
typedef unsigned int uint_std;
typedef unsigned char uint_byte;

const int MAX_DATA_RECORD_SIZE  = 2000000;
const int MAX_ANS_NUM           = 20000000;
const int MIN_PATH_LENGTH       = 3;
const int MAX_PATH_LENGTH       = 7;
const int MAX_ID_NUM            = MAX_DATA_RECORD_SIZE * 2; // Every record has two relatived account
const int PATH_LENGTH_NUM       = MAX_PATH_LENGTH - MIN_PATH_LENGTH + 1;

const int THREAD_NUM            = 4;
const int WRITER_THREAD_NUM     = 4;
const int MAX_ID_STRING_LENGTH  = 12; //log(2 ** 32) + 1
const int ID_HASH_TABLE_SIZE    = 27579263; // MAX_ID_NUM * 2 * 7, bigger is better;

/////////////////////////////////////////////////
////////////////////// Hash Code ////////////////
/////////////////////////////////////////////////
/*
    Here, we face a trade-off about set all byte of id_hash_table to [0xff],
    or, use another table to record whether it be used.
*/

int_std real_ans[THREAD_NUM][PATH_LENGTH_NUM][MAX_ANS_NUM * MAX_PATH_LENGTH];
int_std real_ans_size[THREAD_NUM][PATH_LENGTH_NUM];
int_std ans_pool[PATH_LENGTH_NUM][MAX_ID_NUM];

int_std ans_len[MAX_ANS_NUM];
int_std ans_head[MAX_ANS_NUM];
int_std ans_write_addr[MAX_ANS_NUM];
int_std ans_start_point[MAX_ANS_NUM];
int_std ans_len_size;

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

struct RevData{
    int b, a, c;
};

const int SINGLE_CACHE_LINE_EDGE_NUM = CACHE_LINE_SIZE / (sizeof(int_std));
const int MAX_GRAPH_SIZE = (MAX_ID_NUM) * SINGLE_CACHE_LINE_EDGE_NUM + MAX_DATA_RECORD_SIZE * 3;

struct NodeInfo{
    int first;
    int last;
    int min;
    int max;
};

int_std ALIGNED graph[MAX_GRAPH_SIZE];
NodeInfo ALIGNED node_info[MAX_ID_NUM];

int_std ALIGNED rev_graph[MAX_GRAPH_SIZE];
NodeInfo ALIGNED rev_node_info[MAX_ID_NUM];

inline
void graph_node_edge_init(int_std * graph, NodeInfo * node_info, int * nums, int valid_graph_size){
    node_info[0].min = 0x7fffffff;
    node_info[0].max = 0;
    for (int idx = 1; idx < valid_graph_size; ++idx){
    // In fact, there should be not num equal to zero.
        int num = nums[idx - 1];
        if (unlikely(num == 0)){
            node_info[idx].first = node_info[idx - 1].first;
        } else {
            int store_size = ((2 * num - 1) / SINGLE_CACHE_LINE_EDGE_NUM + 1) * SINGLE_CACHE_LINE_EDGE_NUM;
            node_info[idx].first = node_info[idx - 1].first + store_size;
        }
        node_info[idx].last = node_info[idx].first;
        node_info[idx].min = 0x7fffffff;
        node_info[idx].max = 0;
    }
}

template<class T>
inline
void graph_node_edge_add(int_std * graph, NodeInfo * node_info, Data * tD, int tD_size){
    for (int i = 0; i < tD_size; ++i){
        const auto d = (T *)(tD + i);
        int & end_idx = node_info[d->a].last;
        graph[end_idx] = d->b;
        graph[end_idx + 1] = d->c;
        end_idx += 2;
        node_info[d->a].max = max(d->c, node_info[d->a].max);
        node_info[d->a].min = min(d->c, node_info[d->a].min);
    }
}

struct EdgeData{
    int_std receiver;
    int_std amount;

    bool operator < (const EdgeData & A) const {
        return receiver < A.receiver;
    }
};

struct RevEdgeData{
    int_std receiver;
    int_std amount;

    bool operator < (const RevEdgeData & A) const {
        return receiver > A.receiver;
    }
};

template<class T>
inline 
void graph_node_edge_sort(int_std * graph, NodeInfo * node_info, int valid_graph_size){
    for (int i = 0; i < valid_graph_size; ++i){
        const int first = node_info[i].first, last = node_info[i].last;
        if (last - first > 2) sort((T *)(graph + first), (T *)(graph + last));
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
            if (a == b){continue;}
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
        if (a == b){continue;}
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

    graph_node_edge_init(graph, node_info, out_degree, ids_size);
    graph_node_edge_add<Data>(graph, node_info, tD, tD_size);
    graph_node_edge_sort<EdgeData>(graph, node_info, ids_size);

    graph_node_edge_init(rev_graph, rev_node_info, in_degree, ids_size);
    graph_node_edge_add<RevData>(rev_graph, rev_node_info, tD, tD_size);
    graph_node_edge_sort<RevEdgeData>(rev_graph, rev_node_info, ids_size);
}

/////////////////////////////////////////////////
/////////////////////////////////////////////////

//#define amount_valid(X,Y) (((X)<=5ll*(Y))&&((Y)<=3ll*(X)))
inline
bool amount_check(int X, int Y){
    return (X <= 5ll * Y) && (Y <= 3ll * X);
}

struct BackwardPath{
    int first, second;
    int first_amount, second_amount;

    bool operator<(const BackwardPath & T) const {
        if (first == T.first) return second < T.second;
        return first < T.first;
    }
};

uint_byte used_pool[THREAD_NUM][MAX_ID_NUM];
uint_byte handle_thread_id[MAX_ID_NUM];

//inline
void quick_jump(int_std head, 
                int_std mid,
                vector<vector<BackwardPath>> & jump,
                const int_std * node_list,
                uint_byte used[],
                int_std thread_id,
                int_std mid_amount,
                int_std head_amount){
    const int idx = node_list[0] + 1;
    const auto & jp = jump[mid];
    auto & real_a = real_ans[thread_id][idx];
    auto & real_a_len = real_ans_size[thread_id][idx];
    for (const auto & path : jp){
        if (used[path.first] == false and used[path.second] == false \
            and amount_check(mid_amount, path.first_amount)
            and amount_check(path.second_amount, head_amount)){

            ++ans_pool[idx][head];
            const int node_list_len = node_list[0];
            real_a[real_a_len++] = head;
            for (int i = 1; i <= node_list_len; ++i){
                real_a[real_a_len++] = node_list[i];
            }
            real_a[real_a_len++] = mid;
            real_a[real_a_len++] = path.first;
            real_a[real_a_len++] = path.second;
        }
    }
}

void head_quick_jump(int_std head, 
                vector<vector<BackwardPath>> & jump,
                int_std thread_id){
    const int idx = 0;
    const auto & jp = jump[head];
    auto & real_a = real_ans[thread_id][idx];
    auto & real_a_len = real_ans_size[thread_id][idx];
    for (const auto & path : jp){
        ++ans_pool[idx][head];
        real_a[real_a_len] = head;
        real_a[real_a_len+1] = path.first;
        real_a[real_a_len+2] = path.second;
        real_a_len += 3;
    }
}

////#define EDGE_ITR_INIT(head, itr_u, itr_end) auto itr_u = first_edge[head]; const auto itr_u_end = last_edge[head];

#define EDGE_ITR_INIT(u, v)\
                auto itr_##u = node_info[v].first;\
                const auto itr_end_##u = node_info[v].last;\
                for (; itr_ ##u < itr_end_##u; itr_##u+=2) {if (graph[itr_##u] > head) break;} \
                node_info[v].first = itr_ ##u;

#define GET_NODE_INFO(x)\
                const auto x = graph[itr_##x];\
                const auto amount_##x = graph[itr_##x + 1];

#define TRY_QUICK_JUMP(head, v, amount_head) if(jump_update_flag[v])\
                                                            quick_jump(head, v, jump, node_list, used, thread_id, amount_##v, amount_head)

#define POSSIBLE(x, v) if ((amount_##x > 5ll * node_info[v].max) and (3ll * amount_##x < node_info[v].min)) continue;

void search(int_std head,
            vector<vector<BackwardPath>> & jump,
            const vector<bool> & jump_update_flag,
            uint_byte * used,
            int_std thread_id,
            NodeInfo * node_info){
    if (jump_update_flag[head]) head_quick_jump(head, jump, thread_id);
    int_std node_list[4];
    int_std & node_list_len = node_list[0];
    node_list_len = 0; 
    EDGE_ITR_INIT(u, head);
    for (;itr_u < itr_end_u; itr_u+=2){
        GET_NODE_INFO(u); //u and amount
        POSSIBLE(u, u);
        //cout << amount_u << ' ' << u << ' ' << node_info[0].min << ' ' << node_info[0].max << endl;
        //exit(0);
        used[u] = 1;
        TRY_QUICK_JUMP(head, u, amount_u);
        node_list[++node_list_len] = u;
        EDGE_ITR_INIT(v, u);
        for (;itr_v < itr_end_v; itr_v+=2){
            GET_NODE_INFO(v);
            if (!amount_check(amount_u, amount_v)) continue;
            POSSIBLE(v, v);
            used[v] = 1;
            TRY_QUICK_JUMP(head, v, amount_u);
            node_list[++node_list_len] = v;
            EDGE_ITR_INIT(k, v);
            for (;itr_k < itr_end_k; itr_k+=2){
                GET_NODE_INFO(k);
                if (!amount_check(amount_v, amount_k)) continue;
                if (k == u) continue;
                POSSIBLE(k, k);
                used[k] = 1;
                TRY_QUICK_JUMP(head, k, amount_u);
                node_list[++node_list_len] = k;
                EDGE_ITR_INIT(l, k);
				for (;itr_l < itr_end_l; itr_l+=2){
                    GET_NODE_INFO(l);
                    if (!amount_check(amount_k, amount_l)) continue;
					if (unlikely(jump_update_flag[l] and l != u and l != v)) {
						quick_jump(head, l, jump, node_list, used, thread_id, amount_l, amount_u);
					}
                }
                used[k] = 0;
                node_list_len = 2;
            }
            used[v] = 0;
            node_list_len = 1;
        }
        used[u] = 0;
        node_list_len = 0;
    }
}

int init_jump(int_std head, 
            vector<vector<BackwardPath>> & jump, 
            vector<bool> & jump_update_flag, 
            vector<int_std> & init_node){
    int jump_num = 0;
    init_node.clear();
    auto itr_u = rev_node_info[head].first;
    const auto itr_u_end = rev_node_info[head].last;
    for (; itr_u < itr_u_end && rev_graph[itr_u] > head; itr_u += 2){

        const auto u = rev_graph[itr_u];
        const auto u_amount = rev_graph[itr_u + 1];
        auto itr_v = rev_node_info[u].first;
        const auto itr_v_end = rev_node_info[u].last;
        for (; itr_v < itr_v_end && rev_graph[itr_v] > head; itr_v += 2){

            const auto v = rev_graph[itr_v];
            const auto v_amount = rev_graph[itr_v + 1];

            if (amount_check(v_amount, u_amount)){
                auto itr_mid = rev_node_info[v].first;
                const auto itr_mid_end = rev_node_info[v].last;

                for (; itr_mid < itr_mid_end && rev_graph[itr_mid] >= head; itr_mid += 2){
                    const auto mid = rev_graph[itr_mid];
                    const auto mid_amount = rev_graph[itr_mid + 1];
                    if (amount_check(mid_amount, v_amount) && mid != u){
                        if (head == mid && !amount_check(u_amount, mid_amount)) continue;
                        if (!jump_update_flag[mid]){
                            jump_update_flag[mid] = true;
                            jump[mid].clear();
                            init_node.emplace_back(mid);
                            jump_num+=1;
                        }
                        jump[mid].emplace_back((BackwardPath){v, u, mid_amount, u_amount});
                    }
                }
                //TODO, maybe in here can get path3 directly
            }

        }

    }
    for (const auto & mid : init_node){
        if (jump[mid].size() > 1) {sort(jump[mid].begin(), jump[mid].end());}
    }
    return jump_num;
}

atomic_long handle_num(-1);
void run_job(int_std thread_id, int_std graph_size){
    vector<int_std> init_node;
    NodeInfo thread_node_info[graph_size];
    memcpy(thread_node_info, node_info, graph_size * sizeof(NodeInfo));
    vector<bool> jump_update_flag(graph_size, false);
    vector<vector<BackwardPath>> jump_pool(graph_size);
    #ifdef REQUIRE_DEBUG_INFO
    auto start = chrono::steady_clock::now();
    #endif

    auto & used = used_pool[thread_id];
    for (int t = 0; t < graph_size; ++t){
        int i = handle_num += 1;
        if (i >= graph_size){break;}
        const int jump_num = init_jump(i, jump_pool, jump_update_flag, init_node);
        handle_thread_id[i] = thread_id;
        if (jump_num > 0){
            search(i, jump_pool, jump_update_flag, used, thread_id, thread_node_info);
            for (const auto & mid : init_node){
                jump_update_flag[mid] = false;
            }
        }
    }
    #ifdef REQUIRE_DEBUG_INFO
    auto end = std::chrono::steady_clock::now();
    chrono::duration<double> elapsed_seconds = end-start;
    cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
    #endif
}

void solve(){
    thread threads[THREAD_NUM - 1];
    for (int i = 0; i < THREAD_NUM-1; ++i){
        threads[i] = thread(run_job, i, ids_size);
    }
    run_job(THREAD_NUM - 1, ids_size);
    for (int i = 0; i < THREAD_NUM - 1; ++i){
        threads[i].join();
    }
}


inline
void write_count(const int graph_size, int_std & total_ans, int_std & out_size){
    total_ans = 0;
    out_size = 0;
    for (int len_t = MIN_PATH_LENGTH; len_t < MAX_PATH_LENGTH; ++len_t){
        const int len = len_t;
        const auto idx = len - MIN_PATH_LENGTH;
        const auto & ans = ans_pool[idx];
        int_std start_point_pool[THREAD_NUM][PATH_LENGTH_NUM] = {0};
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
                out_size += id_len[real_a[start_point]] + len;
                out_size += id_len[real_a[start_point + 1]];
                out_size += id_len[real_a[start_point + 2]];
                const int tmp_end_point = start_point + len;
                for (unsigned int i = start_point + 3; i < tmp_end_point; ++i){
                    out_size += id_len[real_a[i]];
                }
                //++out_size;
            }
        }
    }

    {
        const int len = MAX_PATH_LENGTH;
        const auto idx = len - MIN_PATH_LENGTH;
        const auto & ans = ans_pool[idx];
        int_std start_point_pool[THREAD_NUM][PATH_LENGTH_NUM] = {0};
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
            out_size += ans[i] * len;
            const auto & real_a = real_ans[thread_id][idx];
            for (; start_point < end_point; start_point += len){
                out_size += id_len[real_a[start_point + 0]];
                out_size += id_len[real_a[start_point + 1]];
                out_size += id_len[real_a[start_point + 2]];
                out_size += id_len[real_a[start_point + 3]];
                out_size += id_len[real_a[start_point + 4]];
                out_size += id_len[real_a[start_point + 5]];
                out_size += id_len[real_a[start_point + 6]];
                //++out_size;
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

#define WRITE_TASK_BATCH_SIZE 1000
atomic_long write_num(-WRITE_TASK_BATCH_SIZE);
void mmap_multi_thread_writer(const int_std writer_id,
                              char * out_buff){
    //int_std start_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
    //int_std end_point_pool[THREAD_NUM][ANS_LENGTH_NUM] = {0};
    auto beg_t = clock();
    for (int t = 0; t < ans_len_size; ++t){
        int aidx_begin = write_num += WRITE_TASK_BATCH_SIZE;
        int aidx_end = aidx_begin + WRITE_TASK_BATCH_SIZE;
        for (int aidx = aidx_begin; aidx < aidx_end; ++aidx){
            if (aidx >= ans_len_size) {return;}
            const int head = ans_head[aidx];
            const int len = ans_len[aidx];
            const auto idx = len - MIN_PATH_LENGTH;
            int write_offset = ans_write_addr[aidx];
            int_std start_point = ans_start_point[aidx];
            int_std thread_id = handle_thread_id[head];
            const auto & real_a = real_ans[thread_id][idx];
            const int_std end_point = start_point + ans_pool[idx][head] * len;
            for (;start_point < end_point; start_point += len){
                const int a = real_a[start_point];
                memcpy(out_buff + write_offset, id_str[a] + 1, id_str[a][0]);
                write_offset += id_str[a][0];
                for (unsigned int i = start_point + 1; i < start_point + len; ++i){
                    out_buff[write_offset++] = ',';
                    const int a = real_a[i];
                    memcpy(out_buff + write_offset, id_str[a] + 1, id_str[a][0]);
                    write_offset += id_str[a][0];
                }
                out_buff[write_offset++] = '\n';
            }
        }
    }
}

void mmap_write_data(char file_name[], const int graph_size){
    #ifdef REQUIRE_DEBUG_INFO
    auto beg_t = clock();
    #endif
    int_std total_ans, out_size;
    //ans_len.reserve(MAX_ANS_NUM);
    //ans_head.reserve(MAX_ANS_NUM);
    //ans_write_addr.reserve(MAX_ANS_NUM);
    write_count(graph_size, total_ans, out_size);
    //cout << "file size should be: " << out_size << endl;

    int fd = open(file_name, O_CREAT|O_RDWR|O_TRUNC, 0666);
    int flag = ftruncate(fd, out_size);
    char * out_buff = (char*) mmap(NULL, out_size, PROT_WRITE, MAP_SHARED | MAP_POPULATE | MAP_NONBLOCK, fd, 0);

    string total_ans_str = to_string(total_ans);
    int buff_used = total_ans_str.length();
    strcpy(out_buff, total_ans_str.c_str());
    out_buff[buff_used++] = '\n';
    int ans_num_offset = buff_used;

    #ifdef REQUIRE_DEBUG_INFO
    cout << "mmap write count time cost : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    cout << "total_ans: " << total_ans << endl;
    #endif

    thread threads[WRITER_THREAD_NUM];
    for (int i = 0; i < WRITER_THREAD_NUM-1; ++i){
        threads[i] = thread(mmap_multi_thread_writer, i, out_buff + ans_num_offset);
    }
    mmap_multi_thread_writer(WRITER_THREAD_NUM - 1, out_buff + ans_num_offset);
    //mmap_multi_thread_writer(WRITER_THREAD_NUM - 1, out_buff, ans_num_offset);
    for (int i = 0; i < WRITER_THREAD_NUM - 1; ++i){
        threads[i].join();
    }

    #ifdef REQUIRE_DEBUG_INFO
    cout << "mmap write time cost       : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    cout << "total ans is : " << total_ans << endl; 
    #endif
    //munmap(out_buff, out_size);
    //_exit(0);
    close(fd);
}

void memory_report(){
#define INFO_M(x) cout << #x << " is: " << x / (1000 * 1000) << "M" << endl
    INFO_M(MAX_ID_NUM);
    INFO_M(MAX_DATA_RECORD_SIZE);
    INFO_M(MAX_ANS_NUM);
    INFO_M(MAX_GRAPH_SIZE);
    cout << "Graph memory size is: " << (sizeof(graph) >> 20) << "MB" << endl;
    cout << "real_ans memory size is: " << (sizeof(real_ans) >> 20) << "MB" << endl;
    cout << "NodeInfo memory size is " << (sizeof(node_info) >> 20) << "MB" << endl;
}

inline void write_data(char * file_name){
    mmap_write_data(file_name, ids_size);
}

int main(){
    #ifdef REQUIRE_DEBUG_INFO
    memory_report();
    #endif

    #ifdef REQUIRE_DEBUG_INFO
    auto beg_t = clock();
    #endif

    // Delete compile warning!
    char input_file_name[] = INPUT_FILE_NAME;
    quick_input(input_file_name);

    #ifdef REQUIRE_DEBUG_INFO
    cout << "Finished input! The unique id number is: " << ids_size << endl;
    cout << "Input time cost       : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    #endif

    preprocess();
    #ifdef REQUIRE_DEBUG_INFO
    cout << "preprocess time cost       : " << 1. * (clock() - beg_t) / CLOCKS_PER_SEC << endl;
    #endif
    
    solve();

    char output_file_name [] = OUTPUT_FILE_NAME;
    write_data(output_file_name);

    return 0;
}


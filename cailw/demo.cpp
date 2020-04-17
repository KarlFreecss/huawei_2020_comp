#include <cstdio>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>

using namespace std;

typedef unsigned int uint_std;

vector<pair<int, vector<uint_std>>> ans;
unordered_map<uint_std, vector<uint_std>> Graph;
vector<uint_std> empt;
vector<uint_std> node_list;
unordered_map<uint_std, int> used;

unordered_map<uint_std, int> visit;
unordered_map<uint_std, uint_std> color;
unordered_map<uint_std, int> DFN, LOW, group_size;
vector<uint_std> stack;
int tot = 0;
int index = 0;

void tarjan(uint_std x, int dep){
    DFN[x] = LOW[x] = ++tot;
    stack[++index] = x;
    visit[x] = 1;
    for (const auto& v: Graph[x]){
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

void dfs(uint_std u, uint_std s, uint_std len, vector<uint_std> & node_list){
    if (u == s and len > 1){
        if (len >= 3){
            ans.push_back(make_pair(len, node_list));
        }
        return;
    }
    if (len == 7){
        return;
    }

    auto & Gu = Graph[u];
    auto it = lower_bound(Gu.begin(), Gu.end(), s);
    //auto it = Gu.begin();

    for (; it != Gu.end(); ++it){
        const uint_std v = *it;
        //if (v < s or used[v] == 1){// or color[v] != color[u]){
        if (v < s or used[v] == 1){
            continue;
        }
        node_list.push_back(v);
        used[v] = 1;
        dfs(v, s, len+1, node_list);
        used[v] = 0;
        node_list.pop_back();
    }
}

int main(){
    char file_name[] = "/mnt/d/downloads/test_data.txt";
    //char file_name[] = "test_data.txt";
    FILE * fin = fopen(file_name, "r");
    //FILE * fin = fopen("test_data.txt", "r");
    //FILE * fin = fopen("/data/test_data.txt", "r");
    uint_std a, b, c;
    uint_std min_num = 0xffffffff;
    int count = 0;
    while (fscanf(fin, "%u,%u,%u", &a, &b, &c) == 3){
        count++;
        if (Graph.find(a) == Graph.end()){
            Graph[a] = empt;
            if (min_num > a){
                min_num = a;
            }
        }
        Graph[a].push_back(b);
        used[a] = 0;
        used[b] = 0;
        DFN[a] = 0;
        DFN[b] = 0;
    }
    stack.resize(Graph.size());
    for (auto& d : Graph){
        sort(d.second.begin(), d.second.end());
        if (!DFN[d.first]){
            tarjan(d.first, 0);
        }
    }
    for (const auto& d : Graph){
        node_list.clear();
        node_list.push_back(d.first);
        dfs(d.first, d.first, 0, node_list);
    }
    sort(ans.begin(), ans.end());   
    FILE * fout = fopen("new_result.txt", "w");
    //FILE * fout = fopen("/projects/student/result.txt", "w");
    fprintf(fout, "%u\n", ans.size());
    cout << ans.size() << endl;
    for (const auto& d: ans){
        const vector<uint_std> & path = d.second;
        fprintf(fout, "%u", path[0]);
        for (unsigned int i = 1; i + 1< path.size(); ++i){
            fprintf(fout, ",%u", path[i]);
        }
        fprintf(fout, "\n");
    }
    fclose(fin);
    fclose(fout);
    return 0;
}


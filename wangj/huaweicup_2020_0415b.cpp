// huaweicup_2020.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define _CRT_SECURE_NO_WARNINGS 1
#define ID_MAX 9999999 // bu ke neng chu xian zhe me da de ID
#define MAX_EDGES 280000
#define NUM_THREADS 4
//#define DEBUG 1
#define CHECKREP(i,path) (i != path.id[0] && i != path.id[1])
#define CHECKSMALL(i,path) (i < path.id[0] && i < path.id[1])
#define CHECKREP3(i,path) (i != path.id[0] && i != path.id[1] && i != path.id[2])
#define CHECKSMALL3(i,path) (i < path.id[0] && i < path.id[1] && i < path.id[2])

#include <stdio.h>
#ifdef DEBUG
#include <time.h>
#endif
#include <vector>
#include <list>
#include <unordered_map>
#include<algorithm>
#include<thread>  

using namespace std;
typedef unsigned int uname;

class Path {
public:
	int id[3];
	Path(int node) {
		id[0] = node;
		id[1] = ID_MAX;
		id[2] = ID_MAX;
	}
	Path(Path& p, int node) {
		id[0] = p.id[0];
		if (p.id[1] == ID_MAX) {
			id[1] = node;
			id[2] = ID_MAX;
		}
		else {
			id[1] = p.id[1];
			id[2] = node;
		}
	}
	static bool cmp(const Path &a, const Path &b) {
		return a.id[0] < b.id[0];
	}
	static bool cmpInt(const Path& a, const int i) {
		return a.id[0] < i;
	}
};

class ForwardPath {
public:
	int target;
	int pathIdx;
	ForwardPath(int t, int i) : target(t), pathIdx(i) {};
	static bool cmp(const ForwardPath& a, const ForwardPath& b) {
		return a.target < b.target;
	}
	static bool cmpInt(const ForwardPath& a, const int i) {
		return a.target < i;
	}
};

list<int> answer[NUM_THREADS][5]; // bu tong da an jian yong -1 fen ge
int ansCount[NUM_THREADS];
vector<Path>* pathTable[3]; // pathTable[i][j] bao cun le j jing guo i ge dian
// ke yi dao da de dian de lu jing, ji pathTable[0] bao cun le zheng zhang tu
vector<ForwardPath>* forwardTable[3]; // yu ce ke yi hui dao de di fang
vector<uname> id2name;
int vCount;

void loader(char *filePath) {
	vector<uname> edgeIn, edgeOut; // TODO: yu xian fen pei nei cun
	unordered_map<uname, int> name2id;

	FILE* f = fopen(filePath, "r");
	if (f == nullptr) {
		exit(1);
	}
	uname vIn, vOut, amount;
	while (fscanf(f, "%u,%u,%u", &vOut, &vIn, &amount) != EOF) {
		if (vOut == vIn) {
			continue;
		}
		edgeOut.push_back(vOut);
		edgeIn.push_back(vIn);
		name2id[vOut] = -1;
	}
	fclose(f);

	auto unameList = edgeIn;
	sort(unameList.begin(), unameList.end());
	unameList.erase(unique(unameList.begin(), unameList.end()), unameList.end());
	int id = 0;
	for (auto i : unameList) {
		if (name2id.find(i) != name2id.end()) { // i de chu du ye da yu 0
			name2id[i] = id;
			id2name.push_back(i);
			id++;
		}
	}
	vCount = id;

	pathTable[0] = new vector<Path>[vCount];
	int inID, outID;
	for (int i = 0; i < edgeIn.size(); i++) {
		if (name2id.find(edgeIn[i]) == name2id.end() || name2id[edgeOut[i]] == -1) {
			continue;
		}
		inID = name2id[edgeIn[i]];
		outID = name2id[edgeOut[i]];
		pathTable[0][outID].push_back(Path(inID));
	}

	for (int i = 0; i < vCount; i++) {
		sort(pathTable[0][i].begin(), pathTable[0][i].end(), Path::cmp);
		// lu jing you xu yi bao zheng shu chu you xu
	}
}

void pathSearch(int len, int pid) { // len = 1..2
	for (int i = pid; i < vCount; i+=NUM_THREADS) {
		for (int idx1 = 0; idx1 < pathTable[len - 1][i].size(); idx1++) {
			auto p1 = pathTable[len - 1][i][idx1];
			for (auto p2 : pathTable[0][p1.id[len - 1]]) {
				if (i != p2.id[0] && p1.id[0] != p2.id[0]) {
					pathTable[len][i].push_back(Path(p1, p2.id[0]));
					if (CHECKSMALL(p2.id[0], p1) && p2.id[0] < i) {
						forwardTable[len - 1][i].push_back(ForwardPath(p2.id[0], idx1));
					}
				}
			}
		}
		stable_sort(forwardTable[len - 1][i].begin(), forwardTable[len - 1][i].end(), ForwardPath::cmp);
	}
}

void lookForward(int pid) {
	for (int i = pid; i < vCount; i += NUM_THREADS) {
		for (int idx1 = 0; idx1 < pathTable[2][i].size(); idx1++) {
			auto p1 = pathTable[2][i][idx1];
			for (auto p2 : pathTable[0][p1.id[2]]) {
				if (CHECKSMALL3(p2.id[0], p1) && p2.id[0] < i) {
					forwardTable[2][i].push_back(ForwardPath(p2.id[0], idx1));
				}
			}
		}
		stable_sort(forwardTable[2][i].begin(), forwardTable[2][i].end(), ForwardPath::cmp);
	}
}

void outputCircle(int pid, int len, int i, Path* p1, Path* p2 = nullptr) {
	answer[pid][len - 3].push_back(i);
	answer[pid][len - 3].push_back(p1->id[0]);
	answer[pid][len - 3].push_back(p1->id[1]);
	if (len >= 4) {
		answer[pid][len - 3].push_back(p2->id[0]);
	}
	if (len >= 5) {
		answer[pid][len - 3].push_back(p2->id[1]);
	}
	if (len >= 6) {
		answer[pid][len - 3].push_back(p2->id[2]);
	}
	ansCount[pid]++;
}

void circleSearch(int pid) {
	int node3, node4;
	ansCount[pid] = 0;
	for (int i = pid; i < vCount; i += NUM_THREADS) {
		for (auto p1 : pathTable[1][i]) {
			if (!CHECKSMALL(i, p1)) {
				continue;
			}
			node3 = p1.id[1];

			auto it3 = lower_bound(pathTable[0][node3].begin(), pathTable[0][node3].end(), i, Path::cmpInt);
			if (it3 != pathTable[0][node3].end() && it3->id[0] == i) {
				outputCircle(pid, 3, i, &p1);
			}

			auto it = lower_bound(forwardTable[0][node3].begin(), forwardTable[0][node3].end(), i, ForwardPath::cmpInt);
			while (it != forwardTable[0][node3].end() && it->target == i) {
				if (pathTable[0][node3][it->pathIdx].id[0] != p1.id[0]) {
					outputCircle(pid, 4, i, &p1, &pathTable[0][node3][it->pathIdx]);
				}
				it++;
			}

			it = lower_bound(forwardTable[1][node3].begin(), forwardTable[1][node3].end(), i, ForwardPath::cmpInt);
			while (it != forwardTable[1][node3].end() && it->target == i) {
				if (CHECKREP(p1.id[0], pathTable[1][node3][it->pathIdx])) {
					outputCircle(pid, 5, i, &p1, &pathTable[1][node3][it->pathIdx]);
				}
				it++;
			}

			it = lower_bound(forwardTable[2][node3].begin(), forwardTable[2][node3].end(), i, ForwardPath::cmpInt);
			while (it != forwardTable[2][node3].end() && it->target == i) {
				if (CHECKREP3(p1.id[0], pathTable[2][node3][it->pathIdx])) {
					outputCircle(pid, 6, i, &p1, &pathTable[2][node3][it->pathIdx]);
				}
				it++;
			}
		}

		for (auto p1 : pathTable[2][i]) {
			if (!CHECKSMALL3(i, p1)) {
				continue;
			}
			node4 = p1.id[2];

			auto it = lower_bound(forwardTable[2][node4].begin(), forwardTable[2][node4].end(), i, ForwardPath::cmpInt);
			while (it != forwardTable[2][node4].end() && it->target == i) {
				if (CHECKREP3(p1.id[0], pathTable[2][node4][it->pathIdx]) && CHECKREP3(p1.id[1], pathTable[2][node4][it->pathIdx])) {
					answer[pid][4].push_back(i);
					answer[pid][4].push_back(p1.id[0]);
					answer[pid][4].push_back(p1.id[1]);
					answer[pid][4].push_back(p1.id[2]);
					answer[pid][4].push_back(pathTable[2][node4][it->pathIdx].id[0]);
					answer[pid][4].push_back(pathTable[2][node4][it->pathIdx].id[1]);
					answer[pid][4].push_back(pathTable[2][node4][it->pathIdx].id[2]);
					ansCount[pid]++;
				}
				it++;
			}
		}
	}
}

void writer(char *filePath) {
	FILE* f = fopen(filePath, "w");
	if (f == nullptr) {
		exit(2);
	}
	int ans = 0;
	for (int i = 0; i < NUM_THREADS; i++) {
		ans = ans + ansCount[i];
	}
	fprintf(f, "%d\n", ans);
	list<int>::iterator it[NUM_THREADS];
	for (int len = 0; len < 5; len++) {
		for (int i = 0; i < NUM_THREADS; i++) {
			it[i] = answer[i][len].begin();
		}
		int min = 0;
		while (min >= 0) {
			min = 0;
			for (int i = 1; i < NUM_THREADS; i++) {
				if (it[min] == answer[min][len].end() || (it[i] != answer[i][len].end() && *it[i] < *it[min])) {
					min = i;
				}
			}
			if (it[min] == answer[min][len].end()) {
				min = -1;
			}
			else {
				fprintf(f, "%u", id2name[*it[min]]);
				it[min]++;
				for (int i = 0; i < len + 2; i++) {
					fprintf(f, ",%u", id2name[*it[min]]);
					it[min]++;
				}
				fprintf(f, "\n");
			}
		}
	}
}

int main()
{
#ifdef DEBUG
	time_t t1 = time(nullptr);
	char input_path[] = "test_data.txt";
	char output_path[] = "test_result.txt";
#else
	char input_path[] = "/data/test_data.txt";
	char output_path[] = "/projects/student/result.txt";
#endif

	loader(input_path);
#ifdef DEBUG
	time_t t2 = time(nullptr);
#endif
	pathTable[1] = new vector<Path>[vCount];
	pathTable[2] = new vector<Path>[vCount];
	forwardTable[0] = new vector<ForwardPath>[vCount];
	forwardTable[1] = new vector<ForwardPath>[vCount];
	forwardTable[2] = new vector<ForwardPath>[vCount];
	thread threads[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i] = thread(pathSearch, 1, i);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i].join();
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i] = thread(pathSearch, 2, i);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i].join();
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i] = thread(lookForward, i);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i].join();
	}
	
#ifdef DEBUG
	time_t t3 = time(nullptr);
#endif
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i] = thread(circleSearch, i);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i].join();
	}
#ifdef DEBUG
	time_t t4 = time(nullptr);
#endif
	writer(output_path);

#ifdef DEBUG
  time_t t5 = time(nullptr);
  
  FILE* f = fopen("test_time.txt", "w");  
  fprintf(f, "%ld\n%ld\n%ld\n%ld\n%ld\n", t1, t2, t3, t4, t5);
  fprintf(f, "total: %ld\n", t5 - t1);
  fprintf(f, "reader: %ld\n", t2 - t1);
  fprintf(f, "pathSearch: %ld\n", t3 - t2);
  fprintf(f, "circleSearch2: %ld\n", t4 - t3);
  fprintf(f, "writer: %ld\n", t5 - t4);
  fclose(f);
#endif
}

// huaweicup_2020.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define _CRT_SECURE_NO_WARNINGS 1
#define ID_MAX 9999999 // bu ke neng chu xian zhe me da de ID
#define MAX_EDGES 280000
#define BUFFER_SIZE 3
#define DEBUG 1

#include <stdio.h>
#ifdef DEBUG
#include <time.h>
#endif
#include <vector>
#include <list>
#include <unordered_map>
#include<algorithm>

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
	Path(int node, Path &p) {
		id[0] = node;
		id[1] = p.id[0];
		id[2] = p.id[1];
	}
	static bool cmp(Path &a, Path &b) {
		return a.id[0] < b.id[0];
	}
};

list<int> answer[5]; // bu tong da an jian yong -1 fen ge
int ansCount;
vector<Path>* pathTable[BUFFER_SIZE]; // pathTable[i][j] bao cun le j jing guo i ge dian
// ke yi dao da de dian de lu jing, ji pathTable[0] bao cun le zheng zhang tu
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

inline bool checkRep(int id, Path &p) {
	if (id == p.id[0] || id == p.id[1])
		return false;
	return true;
}

inline bool checkSmallest(int id, Path &p) {
	if (id < p.id[0] && id < p.id[1])
		return true;
	return false;
}

void pathSearch(int len) { // len = 1..BUFFER_SIZE
	pathTable[len] = new vector<Path>[vCount];
	for (int i = 0; i < vCount; i++) {
		for (auto p1 : pathTable[0][i]) {
			for (auto p2 : pathTable[len - 1][p1.id[0]]) {
				if (checkRep(i, p2)) {
					pathTable[len][i].push_back(Path(p1.id[0], p2));
				}
			}
		}
	}
}

void circleSearch() {
	int node3, node5;
	ansCount = 0;
	for (int i = 0; i < vCount; i++) {
		for (auto p1 : pathTable[1][i]) {
			if (!checkSmallest(i, p1)) {
				continue;
			}
			node3 = p1.id[1];

			for (auto p2 : pathTable[0][node3]) { //chang 3 de quan
				if (p2.id[0] == i) {
					answer[0].push_back(i);
					answer[0].push_back(p1.id[0]);
					answer[0].push_back(node3);
					answer[0].push_back(-1);
					ansCount++;
				}
			}

			for (auto p2 : pathTable[1][node3]) { //chang 4 de quan
				if (p2.id[1] == i && p2.id[0] > i && p2.id[0] != p1.id[0]) {
					answer[1].push_back(i);
					answer[1].push_back(p1.id[0]);
					answer[1].push_back(node3);
					answer[1].push_back(p2.id[0]);
					answer[1].push_back(-1);
					ansCount++;
				}
			}

			for (auto p2 : pathTable[2][node3]) { //chang 5 de quan
				if (p2.id[2] == i && checkSmallest(i, p2) && checkRep(p1.id[0], p2)) {
					answer[2].push_back(i);
					answer[2].push_back(p1.id[0]);
					answer[2].push_back(node3);
					answer[2].push_back(p2.id[0]);
					answer[2].push_back(p2.id[1]);
					answer[2].push_back(-1);
					ansCount++;
				}
			}

			for (auto p2 : pathTable[1][node3]) {
				if (!checkSmallest(i, p2) || !checkRep(p1.id[0], p2)) {
					continue;
				}
				node5 = p2.id[1];

				for (auto p3 : pathTable[1][node5]) { //chang 6 de quan
					if (p3.id[1] == i && p3.id[0] > i && p3.id[0] != p1.id[0] \
						&& p3.id[0] != node3 && p3.id[0] != p2.id[0]) {
						answer[3].push_back(i);
						answer[3].push_back(p1.id[0]);
						answer[3].push_back(node3);
						answer[3].push_back(p2.id[0]);
						answer[3].push_back(node5);
						answer[3].push_back(p3.id[0]);
						answer[3].push_back(-1);
						ansCount++;
					}
				}

				for (auto p3 : pathTable[2][node5]) { //chang 7 de quan
					if (p3.id[2] == i && checkSmallest(i, p3) && checkRep(p1.id[0], p3) \
						&& checkRep(node3, p3) && checkRep(p2.id[0], p3)) {
						answer[4].push_back(i);
						answer[4].push_back(p1.id[0]);
						answer[4].push_back(node3);
						answer[4].push_back(p2.id[0]);
						answer[4].push_back(node5);
						answer[4].push_back(p3.id[0]);
						answer[4].push_back(p3.id[1]);
						answer[4].push_back(-1);
						ansCount++;
					}
				}
			}


		}
	}
 
}

void writer(char *filePath) {
	FILE* f = fopen(filePath, "w");
	if (f == nullptr) {
		exit(2);
	}
	fprintf(f, "%d\n", ansCount);
	int dot = 0;
	for (int l = 0; l < 5; l++) {
		for (int i : answer[l]) {
			if (i == -1) {
				fprintf(f, "\n");
				dot = 0;
			}
			else if (dot == 0) {
				fprintf(f, "%u", id2name[i]);
				dot = 1;
			}
			else {
				fprintf(f, ",%u", id2name[i]);
				dot = 1;
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
	for (int i = 1; i < BUFFER_SIZE; i++) {
		pathSearch(i);
	}
#ifdef DEBUG
	time_t t3 = time(nullptr);
#endif
	circleSearch();
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

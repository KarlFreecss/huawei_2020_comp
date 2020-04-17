// huaweicup_2020.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define _CRT_SECURE_NO_WARNINGS 1
#define MAX_EDGES 280000
#define BUFFER_SIZE 3
//#define DEBUG 1

#include <stdio.h>
#ifdef DEBUG
#include <time.h>
#endif
#include <vector>
#include <unordered_map>
#include<algorithm>

using namespace std;
typedef unsigned int uname;

class PathNode {
public:
	PathNode* next;
	int id;
	int toward;
	PathNode(int node, int t) : next(nullptr), id(node), toward(t) {};
	PathNode(int node, int t, PathNode* p) : next(p), id(node), toward(t) {};
	static bool cmp(PathNode *a, PathNode *b) {
		return a->id < b->id;
	}
};

typedef PathNode* Path;

vector<Path> answer; // mei ge da an bao kuo liang ge path, fen bie wei lu jing de liang duan
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
		pathTable[0][outID].push_back(new PathNode(inID, inID));
	}

	for (int i = 0; i < vCount; i++) {
		sort(pathTable[0][i].begin(), pathTable[0][i].end(), PathNode::cmp);
		// lu jing you xu yi bao zheng shu chu you xu
	}
}

bool checkPath(int id, Path p) {
	while (p != nullptr) {
		if (p->id == id) {
			return false;
		}
		p = p->next;
	}
	return true;
}

bool checkSmallest(int id, Path p) { // jian cha id shi fou shi quan shang zui xiao de
	while (p != nullptr) {
		if (p->id <= id) {
			return false;
		}
		p = p->next;
	}
	return true;
}

bool checkRep(Path p1, Path p2) { // jian cha shi fou you xiao quan
	Path tmp;
	while (p1->next != nullptr) { // p1 de zui hou yi ge yuan su bu yong jian cha
		tmp = p2;
		while (tmp != nullptr) {
			if (tmp->id == p1->id) {
				return false;
			}
			tmp = tmp->next;
		}
		p1 = p1->next;
	}
	return true;
}

bool checkLink(int out, int in) { // jian cha out ke yi lian dao in
	for (auto i : pathTable[0][out]) {
		if (i->id == in) {
			return true;
		}
	}
	return false;
}

void pathSearch(int len) { // len = 1..BUFFER_SIZE
	pathTable[len] = new vector<Path>[vCount];
	for (int i = 0; i < vCount; i++) {
		for (auto p1 : pathTable[0][i]) {
			for (auto p2 : pathTable[len - 1][p1->id]) {
				if (checkPath(i, p2) > 0) {
					pathTable[len][i].push_back(new PathNode(p1->id, p2->toward, p2));
				}
			}
		}
	}
}

time_t circleSearch() {
	for (int i = 1; i < 3; i++) { // chang du wei 3 huo 4 de quan
		for (int j = 0; j < vCount; j++) {
			for (auto p : pathTable[i][j]) {
				if (checkLink(p->toward, j) && checkSmallest(j, p)) {
					answer.push_back(new PathNode(j, p->toward, p));
					answer.push_back(nullptr);
				}
			}
		}
	}
 
#ifdef DEBUG
	time_t t3h = time(nullptr);
#endif

	int node4;
	for (int i = 0; i < 3; i++) { // chang du wei 5, 6 huo 7 de quan
		for (int j = 0; j < vCount; j++) {
			for (auto p1 : pathTable[2][j]) {
				node4 = p1->toward;
				for (auto p2 : pathTable[i][node4]) {
					if (checkLink(p2->toward, j) && checkSmallest(j, p1) \
						&& checkSmallest(j, p2) && checkRep(p1, p2)) {
						answer.push_back(new PathNode(j, p1->toward, p1));
						answer.push_back(p2);
					}
				}
			}
		}
	}
#ifdef DEBUG
	return(t3h);
#else
  return(0);
#endif
}

void printPath(FILE *f, Path p) {
	while (p != nullptr) {
		fprintf(f, "%u,", id2name[p->id]);
		p = p->next;
	}
}

void writer(char *filePath) {
	FILE* f = fopen(filePath, "w");
	if (f == nullptr) {
		exit(2);
	}
	int ansCount = answer.size()/2;
	fprintf(f, "%d\n", ansCount);
	Path p;
	for (int i = 0; i < ansCount; i++) {
		p = answer[2 * i];
		fprintf(f, "%u", id2name[p->id]);
		p = p->next;
		while (p != nullptr) {
			fprintf(f, ",%u", id2name[p->id]);
			p = p->next;
		}
		p = answer[2 * i + 1];
		while (p != nullptr) {
			fprintf(f, ",%u", id2name[p->id]);
			p = p->next;
		}
		fprintf(f, "\n");
	}
	fclose(f);
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
	time_t t3h = circleSearch();
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
  fprintf(f, "circleSearch1: %ld\n", t3h - t3);
  fprintf(f, "circleSearch2: %ld\n", t4 - t3h);
  fprintf(f, "writer: %ld\n", t5 - t4);
  fclose(f);
#endif
}

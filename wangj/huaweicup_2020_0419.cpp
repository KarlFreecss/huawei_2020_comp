// huaweicup_2020.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define _CRT_SECURE_NO_WARNINGS 1
#define DEBUG 1
#define NUM_THREADS 4
#define MAX_EDGES 280000
#define ID_WIDTH 20
#define IMPOSIBLE_ID ((1 << ID_WIDTH) - 1)

#include <stdio.h>
#include <vector>
#include <list>
#include <unordered_map>
#include<algorithm>
#include<thread>  

#ifdef DEBUG
#include <time.h>
#endif

using namespace std;
typedef unsigned int Uname;
typedef unsigned int Id;
typedef unsigned long long int Path;

const int bufferSize[] = { 30, 200, 1000, 2000 };
const int writeBufferSize[] = { 500000, 1000000, 2000000, 4000000, 6000000 };

inline Path pathSet(Path p, Id i, int idx) {
	return (p & ~((Path)IMPOSIBLE_ID << ID_WIDTH * idx)) + ((Path)i << ID_WIDTH * idx);
}

inline Path pathInit(Id i) {
	return ((Path)i << (ID_WIDTH * 2)) + ((Path)1 << (ID_WIDTH * 2)) - 1;
}

inline Id pathUnpack(Path p, int idx) {
	return (p >> ID_WIDTH * idx) & IMPOSIBLE_ID;
}

inline bool pathCmpInt(Path p, Id id) {
	return (p >> ID_WIDTH * 2) < id;
}

inline bool pathCheckSmall(Path p, Id id) {
	return pathUnpack(p, 0) > id && pathUnpack(p, 1) > id && pathUnpack(p, 2) > id;
}

inline bool pathCheckEq(Path p, Id id) {
	return pathUnpack(p, 0) != id && pathUnpack(p, 1) != id && pathUnpack(p, 2) != id;
}

class ForwardPath {
public:
	Id target;
	Path path;
	ForwardPath(Id t, Path p) : target(t), path(p) {};
	static bool cmp(const ForwardPath& a, const ForwardPath& b) {
		return a.target < b.target;
	}
	static bool cmpInt(const ForwardPath& a, const Id i) {
		return a.target < i;
	}
};

vector<Id> answer[NUM_THREADS][5];
int ansCount[NUM_THREADS];
vector<Path>* pathTable[3]; // pathTable[i][j] bao cun le j jing guo i ge dian
// ke yi dao da de dian de lu jing, ji pathTable[0] bao cun le zheng zhang tu
vector<ForwardPath>* forwardTable[3]; // yu ce ke yi hui dao de di fang
vector<Uname> id2name;
char* writeBuffer[5];
int vCount;

inline Uname getUname(char* buff, int& used) {
	while (buff[used] > 0 && (buff[used] < '0' || buff[used] > '9')) {
		++used;
	}
	if (buff[used] == 0) {
		return IMPOSIBLE_ID;
	}
	Id ret = buff[used++] - '0';
	while (buff[used] >= '0' && buff[used] <= '9') {
		ret = ret * 10 + buff[used++] - '0';
	}
	return ret;
}

void loader(char* filePath) {
	vector<Uname>* edgeIn, * edgeOut;
	int* dIn, * dOut;
	Id* name2Id;

	edgeIn = new vector<Uname>[MAX_EDGES]; // prior knowledge: Uname < 280000
	edgeOut = new vector<Uname>[MAX_EDGES];
	dIn = new int[MAX_EDGES];
	dOut = new int[MAX_EDGES];
	name2Id = new Id[MAX_EDGES];
	for (int i = 0; i < MAX_EDGES; i++) {
		edgeIn[i].reserve(bufferSize[0]);
		edgeOut[i].reserve(bufferSize[0]);
		dIn[i] = 0;
		dOut[i] = 0;
	}

	FILE* f = fopen(filePath, "r");
	if (f == nullptr) {
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	char* input = new char[fileSize + 1];
	rewind(f);
	fread(input, sizeof(char), fileSize, f);
	input[fileSize] = 0;
	fclose(f);

	int readBefore = 0;
	Uname vIn, vOut;
	while (true) {
		vOut = getUname(input, readBefore);
		vIn = getUname(input, readBefore);
		getUname(input, readBefore);
		if (vIn == IMPOSIBLE_ID) {
			break;
		}
		edgeOut[vOut].push_back(vIn);
		edgeIn[vIn].push_back(vOut);
		dIn[vIn]++;
		dOut[vOut]++;
	}
	delete[] input;

	vector<Uname> deleteList;
	deleteList.reserve(MAX_EDGES);
	for (int i = 0; i < MAX_EDGES; i++) {
		if ((dIn[i] > 0 && dOut[i] == 0) || (dIn[i] == 0 && dOut[i] > 0)) {
			deleteList.push_back(i);
		}
	}
	vector<Uname>::iterator cr = deleteList.begin();
	while (cr != deleteList.end()) {
		Uname u = *cr;
		cr++;
		for (Uname uin : edgeIn[u]) {
			dOut[uin]--;
			if (dOut[uin] == 0 && dIn[uin] > 0) {
				deleteList.push_back(uin);
			}
		}
		for (Uname uout : edgeOut[u]) {
			dIn[uout]--;
			if (dIn[uout] == 0 && dOut[uout] > 0) {
				deleteList.push_back(uout);
			}
		}
	}

	Id id = 0;
	id2name.reserve(MAX_EDGES);
	for (int i = 0; i < MAX_EDGES; i++) {
		if (dIn[i] > 0 && dOut[i] > 0) {
			name2Id[i] = id;
			id2name.push_back(i);
			id++;
		}
		else {
			name2Id[i] = IMPOSIBLE_ID;
		}
	}
	vCount = id;

	pathTable[0] = new vector<Path>[vCount];
	Id inId, outId;
	for (int i = 0; i < MAX_EDGES; i++) {
		outId = name2Id[i];
		if (outId == IMPOSIBLE_ID) {
			continue;
		}
		pathTable[0][outId].reserve(bufferSize[0]);
		for (Uname u : edgeOut[i]) {
			inId = name2Id[u];
			if (inId != IMPOSIBLE_ID) {
				pathTable[0][outId].push_back(pathInit(inId));
			}
		}
	}
	delete[] edgeOut;
	delete[] edgeIn;
	delete[] dIn;
	delete[] dOut;
	delete[] name2Id;

	for (int i = 0; i < vCount; i++) {
		sort(pathTable[0][i].begin(), pathTable[0][i].end());
	}
}

void pathSearch(int len, int pid) { // len = 1..2
	for (Id i = pid; i < vCount; i += NUM_THREADS) {
		pathTable[len][i].reserve(bufferSize[len]);
		forwardTable[len - 1][i].reserve(bufferSize[len] * i / vCount);
		for (auto p1 : pathTable[len - 1][i]) {
			for (auto p2 : pathTable[0][pathUnpack(p1, 3 - len)]) {
				if (i != pathUnpack(p2, 2) && pathUnpack(p1, 2) != pathUnpack(p2, 2)) {
					pathTable[len][i].push_back(pathSet(p1, pathUnpack(p2, 2), 2 - len));
					if (pathCheckSmall(p1, pathUnpack(p2, 2)) && pathUnpack(p2, 2) < i) {
						forwardTable[len - 1][i].push_back(ForwardPath(pathUnpack(p2, 2), p1));
					}
				}
			}
		}
		stable_sort(forwardTable[len - 1][i].begin(), forwardTable[len - 1][i].end(), ForwardPath::cmp);
	}
}

void lookForward(int pid) {
	for (Id i = pid; i < vCount; i += NUM_THREADS) {
		forwardTable[2][i].reserve(bufferSize[3] * i / vCount);
		for (auto p1 : pathTable[2][i]) {
			for (auto p2 : pathTable[0][pathUnpack(p1, 0)]) {
				if (pathCheckSmall(p1, pathUnpack(p2, 2)) && pathUnpack(p2, 2) < i) {
					forwardTable[2][i].push_back(ForwardPath(pathUnpack(p2, 2), p1));
				}
			}
		}
		stable_sort(forwardTable[2][i].begin(), forwardTable[2][i].end(), ForwardPath::cmp);
	}
}

inline void outputCircle(int pid, int len, int i, Path p1, Path p2 = 0) {
	answer[pid][len].push_back(i);
	answer[pid][len].push_back(pathUnpack(p1, 2));
	answer[pid][len].push_back(pathUnpack(p1, 1));
	if (len >= 1) {
		answer[pid][len].push_back(pathUnpack(p2, 2));
	}
	if (len >= 2) {
		answer[pid][len].push_back(pathUnpack(p2, 1));
	}
	if (len >= 3) {
		answer[pid][len].push_back(pathUnpack(p2, 0));
	}
	ansCount[pid]++;
}

void circleSearch(int pid) {
	Id node3, node4;
	ansCount[pid] = 0;
	for (Id i = pid; i < vCount; i += NUM_THREADS) {
		for (auto p1 : pathTable[1][i]) {
			if (!pathCheckSmall(p1, i)) {
				continue;
			}
			node3 = pathUnpack(p1, 1);

			auto it3 = lower_bound(pathTable[0][node3].begin(), pathTable[0][node3].end(), i, pathCmpInt);
			if (it3 != pathTable[0][node3].end() && pathUnpack(*it3, 2) == i) {
				outputCircle(pid, 0, i, p1);
			}

			auto it = lower_bound(forwardTable[0][node3].begin(), forwardTable[0][node3].end(), i, ForwardPath::cmpInt);
			while (it != forwardTable[0][node3].end() && it->target == i) {
				if (pathUnpack(it->path, 2) != pathUnpack(p1, 2)) {
					outputCircle(pid, 1, i, p1, it->path);
				}
				it++;
			}

			it = lower_bound(forwardTable[1][node3].begin(), forwardTable[1][node3].end(), i, ForwardPath::cmpInt);
			while (it != forwardTable[1][node3].end() && it->target == i) {
				if (pathCheckEq(it->path, pathUnpack(p1, 2))) {
					outputCircle(pid, 2, i, p1, it->path);
				}
				it++;
			}

			it = lower_bound(forwardTable[2][node3].begin(), forwardTable[2][node3].end(), i, ForwardPath::cmpInt);
			while (it != forwardTable[2][node3].end() && it->target == i) {
				if (pathCheckEq(it->path, pathUnpack(p1, 2))) {
					outputCircle(pid, 3, i, p1, it->path);
				}
				it++;
			}
		}

		for (auto p1 : pathTable[2][i]) {
			if (!pathCheckSmall(p1, i)) {
				continue;
			}
			node4 = pathUnpack(p1, 0);

			auto it = lower_bound(forwardTable[2][node4].begin(), forwardTable[2][node4].end(), i, ForwardPath::cmpInt);
			while (it != forwardTable[2][node4].end() && it->target == i) {
				if (pathCheckEq(it->path, pathUnpack(p1, 2)) && pathCheckEq(it->path, pathUnpack(p1, 1))) {
					answer[pid][4].push_back(i);
					answer[pid][4].push_back(pathUnpack(p1, 2));
					answer[pid][4].push_back(pathUnpack(p1, 1));
					answer[pid][4].push_back(pathUnpack(p1, 0));
					answer[pid][4].push_back(pathUnpack(it->path, 2));
					answer[pid][4].push_back(pathUnpack(it->path, 1));
					answer[pid][4].push_back(pathUnpack(it->path, 0));
					ansCount[pid]++;
				}
				it++;
			}
		}
	}
}

void writer(char* buffer, int len) {
	vector<Id>::iterator it[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; i++) {
		it[i] = answer[i][len].begin();
	}
	int min = 0;
	int bias = 0;
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
			bias += sprintf(buffer + bias, "%u", id2name[*it[min]]);
			it[min]++;
			for (int i = 0; i < len + 2; i++) {
				bias += sprintf(buffer + bias, ",%u", id2name[*it[min]]);
				it[min]++;
			}
			bias += sprintf(buffer + bias, "\n");
		}
	}
}

int main()
{
#ifdef DEBUG
	clock_t t1 = clock();
	char input_path[] = "test_data.txt";
	char output_path[] = "test_result.txt";
#else
	char input_path[] = "/data/test_data.txt";
	char output_path[] = "/projects/student/result.txt";
#endif

	loader(input_path);

#ifdef DEBUG
	clock_t t2 = clock();
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
	clock_t t3 = clock();
#endif

	for (int i = 0; i < NUM_THREADS; ++i) {
		for (int j = 0; j < 5; j++) {
			answer[i][j].reserve(writeBufferSize[j]);
		}
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i] = thread(circleSearch, i);
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i].join();
	}

#ifdef DEBUG
	clock_t t4 = clock();
#endif

	for (int i = 0; i < NUM_THREADS; ++i) {
		int ansLen = 0;
		for (int j = 0; j < NUM_THREADS; j++) {
			ansLen += answer[j][i].size();
		}
		if (ansLen != 0) {
			writeBuffer[i] = new char[10 * (ansLen + 1)];
			threads[i] = thread(writer, writeBuffer[i], i);
		}
		else {
			writeBuffer[i] = nullptr;
		}
	}
	for (int i = 0; i < NUM_THREADS; ++i) {
		if (writeBuffer[i] != nullptr) {
			threads[i].join();
		}
	}
	int ansLen = 0;
	for (int j = 0; j < NUM_THREADS; j++) {
		ansLen += answer[j][4].size();
	}
	if (ansLen != 0) {
		writeBuffer[4] = new char[10 * (ansLen + 1)];
		threads[0] = thread(writer, writeBuffer[4], 4); // jin NUM_THREADS = 4 shi zheng que
	}
	else {
		writeBuffer[4] = nullptr;
	}
	FILE* f = fopen(output_path, "w");
	if (f == nullptr) {
		exit(2);
	}
	int ans = 0;
	for (int i = 0; i < NUM_THREADS; i++) {
		ans = ans + ansCount[i];
	}
	fprintf(f, "%d\n", ans);
	for (int i = 0; i < NUM_THREADS; ++i) {
		if (writeBuffer[i] != nullptr) {
			fprintf(f, "%s", writeBuffer[i]);
		}
	}
	if (writeBuffer[4] != nullptr) {
		threads[0].join();
		fprintf(f, "%s", writeBuffer[4]);
	}
	fclose(f);

#ifdef DEBUG
  clock_t t5 = clock();
  
  FILE* ftest = fopen("test_time.txt", "w");  
  fprintf(ftest, "%ld\n%ld\n%ld\n%ld\n%ld\n", t1, t2, t3, t4, t5);
  fprintf(ftest, "total: %ld\n", t5 - t1);
  fprintf(ftest, "reader: %ld\n", t2 - t1);
  fprintf(ftest, "pathSearch: %ld\n", t3 - t2);
  fprintf(ftest, "circleSearch: %ld\n", t4 - t3);
  fprintf(ftest, "writer: %ld\n", t5 - t4);
  fclose(ftest);
#endif
}

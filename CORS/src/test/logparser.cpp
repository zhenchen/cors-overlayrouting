#include <stdio.h>		// fread(), fwrite()
#include <unistd.h>		// getopt()
#include <arpa/inet.h>	// ntohs()

#include <iostream>
#include <vector>
#include <map>

using namespace std;

const int MAX_LEN = 2048;
const long MAX_TIME = 0xffffffff;

typedef struct Event {
	timeval ts;
	int len;
	char pkt[MAX_LEN];
} Event;

//typedef map<int, Event> EventMap;
typedef vector<Event> Events;

void PrintUsage() {
	cerr << "Usage: ./logparser -i inputfile -o outputfile" << endl;
}

int GetSeq(char* pkt, int len) {
	if (len <= 4) {
		cerr << "packet too short" << endl;
		return -1;
	}
	int seq = 0;
	memcpy(&seq, pkt+2, 2);
	return ntohs(seq);
}

int Parse(const string &input) {
	FILE *fp = fopen(input.c_str(), "rb");
	if (!fp) {
		cerr << "cannot open file: " << input << endl;
		return -1;
	}
	Events events;
	Event event;
	int seq;
	vector<int> seq_vec;
	fread(&event.ts, sizeof(timeval), 1, fp);
	while (!(event.ts.tv_sec == MAX_TIME && 
			 event.ts.tv_usec == MAX_TIME)) {
		fread(&event.len, sizeof(int), 1, fp);
		fread(event.pkt, 1, event.len, fp);
		seq = GetSeq(event.pkt, event.len);
		events.push_back(event);
		seq_vec.push_back(seq);
		fread(&event.ts, sizeof(timeval),1, fp);
	}
	cout << "number of packets: " << events.size() << endl;
	for (int i = 0; i < events.size(); i++) {
		cout << "ts = " << events[i].ts.tv_sec << "\t"
			 << events[i].ts.tv_usec << "\t"
			 << "seq = " << seq_vec[i] << "\t"
			 << "len = " << events[i].len << endl;
	}
	fclose(fp);
	return 0;
}

int main(int argc, char *argv[]) {
	std::string filename = argv[1];
	Parse(filename);
	return 0;
}

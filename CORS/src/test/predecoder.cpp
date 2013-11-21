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

typedef map<int, Event> EventMap;
typedef vector<Event> Events;

void PrintUsage() {
	cerr << "Usage: ./predecoder -i inputfile -o outputfile" << endl;
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

int ReadRtpData(const string &input,
				EventMap *event_map_ptr) {
	FILE *fp = fopen(input.c_str(), "rb");
	if (!fp) {
		cerr << "cannot open file: " << input << endl;
		return -1;
	}
	event_map_ptr->clear();
	//Events events;
	Event event;
	int seq;
	//vector<int> seq_vec;
	fread(&event.ts, sizeof(timeval), 1, fp);
	while (!(event.ts.tv_sec == MAX_TIME && 
			 event.ts.tv_usec == MAX_TIME)) {
		fread(&event.len, sizeof(int), 1, fp);
		fread(event.pkt, 1, event.len, fp);
		seq = GetSeq(event.pkt, event.len);
		// insertion will check if the same seq exists
		event_map_ptr->insert(pair<int, Event>(seq, event));
		//events.push_back(event);
		//seq_vec.push_back(seq);
		fread(&event.ts, sizeof(timeval),1, fp);
	}
	// for (int i = 0; i < events.size(); i++) {
	// 	cout << "ts = " << events[i].ts.tv_sec << "\t"
	// 		 << events[i].ts.tv_usec << "\t"
	// 		 << "seq = " << seq_vec[i] << "\t"
	// 		 << "len = " << events[i].len << endl;
	// }
	// EventMap::iterator it = event_map_ptr->begin();
	// for (; it != event_map_ptr->end(); ++it) {
	// 	cout << "seq = " << it->first << "\t"
	// 		 << "ts = " << it->second.ts.tv_sec << "\t"
	// 		 << it->second.ts.tv_usec << "\t"
	// 		 << "len = " << it->second.len << endl;
	// }
	// fclose(fp);
	return 0;
}

int WriteRtpData(const string &output,
				 const EventMap &event_map) {
	FILE *fp = fopen(output.c_str(), "wb");
	if (!fp) {
		cerr << "cannot open file: " << output << endl;
		return -1;
	}
	EventMap::const_iterator it = event_map.begin();
	for (; it != event_map.end(); ++it) {
		fwrite(&it->second.ts, sizeof(timeval), 1, fp);
		fwrite(&it->second.len, sizeof(int), 1, fp);
		fwrite(&it->second.pkt, 1, it->second.len, fp);
	}
	timeval tm_fin;
	tm_fin.tv_sec = tm_fin.tv_usec = MAX_TIME;
	fwrite(&tm_fin, sizeof(timeval), 1, fp);
	fclose(fp);
	return 0;
}

int main(int argc, char *argv[]) {
	int c;
	std::string input, output;

	while((c = getopt(argc, argv, "i:o:")) != -1) {
		switch(c) {
		case 'i':
			input = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case '?':
			PrintUsage();
			break;
		}
	}
	if (input.empty()) {
		cout << "must specify input filename" << endl;
		PrintUsage();
		return -1;
	}
	if (output.empty()) {
		cout << "must specify output filename" << endl;
		PrintUsage();
		return -1;
	}
	cout << "input file: " << input << endl;
	cout << "output file: " << output << endl;

	EventMap event_map;
	ReadRtpData(input, &event_map);
	WriteRtpData(output, event_map);

	return 0;
}

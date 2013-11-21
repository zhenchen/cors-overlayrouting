#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>

#include <iostream>
#include <vector>

#include "../client/corssocket.h"
#include "../utils/utilfunc.h"

using namespace std;

const int MAX_LEN = 2048;
const short DEFAULT_PORT = 5000;
const uint8_t DEFAULT_RLNUM = 1;
const uint32_t DEFAULT_DELAY = 300;
const long MAX_TIME = 0xffffffff;

typedef long long time64;
typedef struct Event {
	timeval ts;
	int len;
	char pkt[MAX_LEN];
} Event;
typedef vector<Event> Events;

void PrintUsage() {
	cerr << "usage: ./rtpreceiver [option]... file" << endl;
	cerr << "\t-c: use CORS API" << endl;
	cerr << "\t-d time: delay requirement in ms" << endl;
	cerr << "\t-l port: local port number" << endl;
	cerr << "\t-p pathnum: number of relay paths" << endl;
	cerr << "\t-r ip:port: remote port number" << endl;
}

timeval CalcDelta(const timeval &tm1, const timeval &tm2) {
	timeval delta;
	assert(tm1.tv_sec > tm2.tv_sec ||
		   (tm1.tv_sec == tm2.tv_sec &&
			tm1.tv_usec >= tm2.tv_usec));
	if (tm1.tv_usec >= tm2.tv_usec) {
		delta.tv_usec = tm1.tv_usec - tm2.tv_usec;
		delta.tv_sec = tm1.tv_sec - tm2.tv_sec;
	} else {
		delta.tv_usec = 1000000 + tm1.tv_usec - tm2.tv_usec;
		delta.tv_sec = tm1.tv_sec - tm2.tv_sec - 1;
	}
	return delta;
}

int WriteRtpData(FILE *fp, const timeval &ts,
				 const int len, const char* pkt) {
	fwrite(&ts, sizeof(ts), 1, fp);
  	fwrite(&len, sizeof(len), 1, fp);
  	fwrite(pkt, 1, len, fp);
	return 0;
}

int InitSock(const sockaddr_in &local) {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(sock, (sockaddr *)&local, sizeof(local)) < 0) {
		cerr << "binding socket error" << endl;
		close(sock);
		return -1;
	}
	return sock;
}

int main(int argc, char *argv[]) {
	int c;
	bool use_cors = false;
	std::string remote_str, filename;
	sockaddr_in local, remote;
	timeval timestamp, tm_start, tm_delta;
	uint32_t delay = DEFAULT_DELAY;
	uint8_t relaynum = DEFAULT_RLNUM;

	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(DEFAULT_PORT);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	while((c = getopt(argc, argv, "cd:l:p:r:")) != -1 ) {
		switch(c) {
		case 'c':
			use_cors = true;
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'l':
			local.sin_port = htons(atoi(optarg));
			break;
		case 'p':
			relaynum = atoi(optarg);
			break;
		case 'r':
			remote_str = optarg;
			break;
		case '?':
			PrintUsage();
			break;
		}
	}
	if (optind != argc - 1) {
		cout << "must specify an input file" << endl;
		PrintUsage();
		return -1;
	}
	filename = argv[optind];

	if (remote_str.empty()) {
		cout << "must specify remote addr -- ip:port" << endl;
		PrintUsage();
		return -1;
	}
	size_t pos = remote_str.find(":");
	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;
	remote.sin_port = htons(atoi(remote_str.substr(pos+1).c_str()));
	remote.sin_addr.s_addr = inet_addr(remote_str.substr(0, pos).c_str());

	cout << "use_cors: " << (use_cors ? "yes" : "no") << endl;
	cout << "local addr: " << sockaddr2str(local) << endl;
	cout << "remote addr: " << sockaddr2str(remote) << endl;
	cout << "delay requirement: " << delay << "ms" << endl;
	cout << "relay path number: " << (int)relaynum << endl;
	cout << "output file: " << filename << endl;

	FILE *fp = fopen(filename.c_str(), "wb");
	if (!fp) {
		cerr << "cannot open output file " << filename << endl;
		return -1;
	}

	int sock;
	if ((sock = InitSock(local)) < 0) {
		return -1;
	}

	Requirement req;
	memset(&req, 0, sizeof(req));
	req.delay = delay;
	req.relaynum = relaynum;

	Feature feature;
	memset(&feature, 0, sizeof(feature));
	feature.delay = delay;
	feature.significance = 0;
	feature.seculevel = 0;
	feature.channelnum = relaynum;

	vector<string> rsparams, mmparams;
	rsparams.push_back("SimpleRlysltr");
	rsparams.push_back("rsltr.conf");
	rsparams.push_back("500000");
	rsparams.push_back("300");

	mmparams.push_back("SimpleMpathManager");

	CorsSocket cors_sk(sock, rsparams, mmparams);
	cors_sk.init(req, remote.sin_addr.s_addr, remote.sin_port);

	fd_set rfds;
	char rbuff[MAX_LEN];
	int rlen = -1;
	sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	timeval timeout;
	int stop = 0;
	bool first = true;

	while (true) {
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		int ret = select(sock + 1, &rfds, NULL, NULL, &timeout);
		if (ret < 0) {
			cerr << "select error" << endl;
			break;
		} else if (ret == 0) {
			break;
		} else {
			if (use_cors) {
				rlen = -1;
				cors_sk.recvfrom(rbuff, rlen, from.sin_addr.s_addr, from.sin_port);
				if (rlen < 0) {
					stop++;
					if (stop == 20) break;
					continue;
				} else {
					stop = 0;
				}
			} else {
				rlen = recvfrom(sock, rbuff, MAX_LEN, 0, (sockaddr *)&from, &fromlen);
			}
			// write rbuff to log file
			// (TODO)remember to minus the start time
			gettimeofday(&timestamp, NULL);
			if (first) {
				tm_start.tv_sec = timestamp.tv_sec;
				tm_start.tv_usec = timestamp.tv_usec;
				tm_delta.tv_sec = 0;
				tm_delta.tv_usec = 0;
				first = false;
			} else {
				tm_delta = CalcDelta(timestamp, tm_start);
			}
			WriteRtpData(fp, tm_delta, rlen, rbuff);
		}
	}
	timeval tm_fin;
	tm_fin.tv_sec = MAX_TIME;
	tm_fin.tv_usec = MAX_TIME;
	fwrite(&tm_fin, sizeof(tm_fin), 1, fp);
	fclose(fp);
	cors_sk.close();
	close(sock);
  cout << "Finished. Exit..." << endl;
	return 0;
}

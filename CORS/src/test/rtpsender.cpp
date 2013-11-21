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
#include <fcntl.h>

#include <iostream>
#include <iomanip>
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
	timeval delta;
	int len;
	char pkt[MAX_LEN];
} Event;
typedef vector<Event> Events;

void PrintUsage() {
	cerr << "usage: ./rtpsender -r ip:port [option]... file" << endl;
	cerr << "\t-c: use CORS API" << endl;
	cerr << "\t-d time: delay requirement in ms" << endl;
	cerr << "\t-l port: local port number" << endl;
	cerr << "\t-n count: replay times of input" << endl;
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

int ReadRtpData(const string &filename,
				Events *events_ptr) {
	FILE *fp = fopen(filename.c_str(), "rb");
	if (!fp) {
		cerr << "cannot open file: " << filename << endl;
		return -1;
	}
	Event event;
	fread(&event.ts, sizeof(timeval), 1, fp);
	while (!(event.ts.tv_sec == MAX_TIME && 
			 event.ts.tv_usec == MAX_TIME)) {
		fread(&event.len, sizeof(int), 1, fp);
		fread(event.pkt, 1, event.len, fp);
		events_ptr->push_back(event);
		fread(&event.ts, sizeof(timeval), 1, fp);
	} 
	cout << "number of rtp packets: " << events_ptr->size() << endl;
	// calculate delta
	Events::iterator it = events_ptr->begin() + 1;
	for (; it != events_ptr->end(); ++it) {
		it->delta = CalcDelta(it->ts, (it-1)->ts);
	}
	events_ptr->begin()->delta = (events_ptr->end()-1)->delta;
//	for (it = events_ptr->begin(); it != events_ptr->end(); ++it) {
//		cout << "ts = " << it->ts.tv_sec << "\t"
//			 << it->ts.tv_usec << "\t"
//			 << "dt = " << it->delta.tv_usec << "\t"
//			 << "len = " << it->len << endl;
//	}
	fclose(fp);
	return 0;
}

int InitSock(const sockaddr_in &local)
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(sock, (sockaddr *)&local, sizeof(local)) < 0) {
		cerr << "binding socket error" << endl;
		close(sock);
		return -1;
	}
	return sock;
}

pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t timer_cond = PTHREAD_COND_INITIALIZER;
int timer_sig = -1;
int replay = 1;		

void *timer_thread(void *param) {
	Events *events_ptr = static_cast<Events *>(param);
	Events::iterator it;
	int index = 0;
	for (; replay > 0; replay--) {
		for (it = events_ptr->begin(), index = 0;
			 it != events_ptr->end(); index++, ++it) {
			timeval timeout = it->delta;
			select(0, NULL, NULL, NULL, &timeout);
			pthread_mutex_lock(&timer_mutex);
			timer_sig = index;
			pthread_cond_signal(&timer_cond);
			pthread_mutex_unlock(&timer_mutex);
		}
	}
	usleep(1000000);
	pthread_mutex_lock(&timer_mutex);
	timer_sig = -2;
	pthread_cond_signal(&timer_cond);
	pthread_mutex_unlock(&timer_mutex);
	return NULL;
}

void *recv_thread(void *param) {
	CorsSocket *cors = static_cast<CorsSocket *>(param);
	fd_set rfds;
	char rbuff[MAX_LEN];
	int rlen = -1;
	sockaddr_in from;
	int sk = cors->get_sock();
	while (true) {
		FD_ZERO(&rfds);
		FD_SET(sk, &rfds);
		int ret = select(sk + 1, &rfds, NULL, NULL, NULL);
		if (ret < 0) {
			cerr << "select error in recv_thread" << endl;
			break;
		} else {
			rlen = -1;
			cors->recvfrom(rbuff, rlen, from.sin_addr.s_addr, from.sin_port);
		}
	}
	return NULL;
}

struct Arguments {
  CorsSocket *cors;
  char *buf;
  int len;
  sockaddr_in *addr;
  Feature *feature;
};

void *send_thread(void *param) {
  timeval tmp;
  gettimeofday(&tmp, NULL);
  cout << "#### enter thread at " << tmp.tv_sec
       << "." << setw(6) << setfill('0') << tmp.tv_usec << endl;

  Arguments *arg = static_cast<Arguments *>(param);
  arg->cors->sendto(arg->buf, arg->len,
                    arg->addr->sin_addr.s_addr,
                    arg->addr->sin_port,
                    *(arg->feature));
  delete arg;
  gettimeofday(&tmp, NULL);
  cout << "#### leave thread at " << tmp.tv_sec
       << "." << setw(6) << setfill('0') << tmp.tv_usec << endl;
  return NULL;
}

int main(int argc, char *argv[]) {
	int c;
	bool use_cors = false;
	std::string remote_str, filename;
	sockaddr_in local, remote;
	timeval tm_start, tm_end;
	uint32_t delay = DEFAULT_DELAY;
	uint8_t relaynum = DEFAULT_RLNUM;

	// default arguments
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(DEFAULT_PORT);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	replay = 1;

	while((c = getopt(argc, argv, "cd:l:n:p:r:")) != -1) {
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
		case 'n':
			replay = atoi(optarg);
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
	cout << "replay time: " << replay << endl;
	cout << "delay requirement: " << delay << "ms" << endl;
	cout << "relay path number: " << (int)relaynum << endl;
	cout << "input file: " << filename << endl; 

	Events events;
	if (ReadRtpData(filename, &events) < 0) {
		return -1;
	}

	int sock;
	if ((sock = InitSock(local)) < 0) {
		return -1;
	}

  int flags;
  if ((flags = fcntl(sock, F_GETFL, 0)) < 0) {
    cerr << "fcntl(GETFL) error" << endl;
    return -1;
  }

  if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
    cerr << "fcntl(SETFL) error" << endl;
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
	rsparams.push_back("300000");	// probe interval
	rsparams.push_back("300");	// timeout seconds

	mmparams.push_back("SimpleMpathManager");

	CorsSocket *cors_sk = NULL;
  if (use_cors) {
    cors_sk = new CorsSocket(sock, rsparams, mmparams);
	  cors_sk->init(req, remote.sin_addr.s_addr, remote.sin_port);
  }

	pthread_t recv_tid, timer_tid;
	if (use_cors) {
		pthread_create(&recv_tid, NULL, recv_thread, cors_sk);
	}
	// pthread_create(&timer_tid, NULL, timer_thread, &events);

	int index = 0;
  int sended = 0;
  int target = 0;
  timeval tm_used, tm_begin;
  // gettimeofday(&tm_begin, NULL);
	Events::iterator it;
	for (; replay > 0; replay--) {
		for (it = events.begin(); it != events.end(); ++it) {
			timeval timeout = it->delta;
			select(0, NULL, NULL, NULL, &timeout);
      if (use_cors) {
        cors_sk->sendto(it->pkt, 
                       it->len,
                       remote.sin_addr.s_addr,
                       remote.sin_port,
                       feature);
      } else {
        sendto(sock, it->pkt, it->len, 0, 
               (sockaddr *)&remote, sizeof(remote));
      }
      sended++;
		}
	}
#if 0
	while (true) {
		pthread_mutex_lock(&timer_mutex);
		while (timer_sig == -1) {
			pthread_cond_wait(&timer_cond, &timer_mutex);
		}
		index = timer_sig;
		timer_sig = -1;
		pthread_mutex_unlock(&timer_mutex);
		
    if (index == -2) {
      target = events.size();
    } else {
      target = index + 1;
    }
    for (int i = sended; i < target; i++) {
		  //gettimeofday(&tm_start, NULL);
      // tm_used = CalcDelta(tm_start, tm_begin);
      //cout << "#### main() send " << i << " at " << tm_start.tv_sec
      //     << "." << setw(6) << setfill('0') << tm_start.tv_usec << endl;
      if (use_cors) {
		    // gettimeofday(&tm_start, NULL);
        // tm_used = CalcDelta(tm_start, tm_begin);
        // cout << "#### main() send " << i << " at " << tm_start.tv_sec
        //     << "." << setw(6) << setfill('0') << tm_start.tv_usec << endl;
        // Arguments *arg = new Arguments;
        // arg->cors = &cors_sk;
        // arg->buf = events[i].pkt;
        // arg->len = events[i].len;
        // arg->addr = &remote;
        // arg->feature = &feature;
        // pthread_t send_tid;
        // pthread_create(&send_tid, NULL, send_thread, arg);
        cors_sk.sendto(events[i].pkt,
                       events[i].len,
                       remote.sin_addr.s_addr,
                       remote.sin_port,
                       feature);
        
		    // gettimeofday(&tm_end, NULL);
        // cout << "#### main() finish send at " << tm_end.tv_sec
        //      << "." << setw(6) << setfill('0') << tm_end.tv_usec << endl;
        // tm_used = CalcDelta(tm_end, tm_start);
        // cout << "#### main() time_used = " << tm_used.tv_sec
        //      << "." << setw(6) << setfill('0') << tm_used.tv_usec << endl;
      } else {
		    // gettimeofday(&tm_start, NULL);
        // tm_used = CalcDelta(tm_start, tm_begin);
        // cout << "#### main() send " << i << " at " << tm_start.tv_sec
        //     << "." << setw(6) << setfill('0') << tm_start.tv_usec << endl;
        sendto(sock, events[i].pkt,
               events[i].len, 0,
               (sockaddr *)&remote, sizeof(remote));
		    // gettimeofday(&tm_end, NULL);
        // cout << "#### main() finish send at " << tm_end.tv_sec
        //      << "." << setw(6) << setfill('0') << tm_end.tv_usec << endl;
        // tm_used = CalcDelta(tm_end, tm_start);
        // cout << "#### main() time_used = " << tm_used.tv_sec
        //      << "." << setw(6) << setfill('0') << tm_used.tv_usec << endl;
      }
      sended++;
    }
    if (index == -2) 
      break;

		// if (index == -2) {
    //   for (int i = sended; i < events.size)
    //   break;
    // } else {
    //   for (int i = sended; i <= index; i++) {
		// 	  if (use_cors)
		// 		  cors_sk.sendto(events[i].pkt,
		// 					           events[i].len,
		// 					           remote.sin_addr.s_addr,
		// 					           remote.sin_port,
		// 					           feature);
		// 	  else
		// 		  sendto(sock, events[i].pkt,
		// 			       events[i].len, 0,
		// 			       (sockaddr *)&remote, sizeof(remote));
    //     sended++;
    //   }
		// }
		// cout << "time_used main = " 
		//	    << tm_end.tv_usec - tm_start.tv_usec << endl;
	}
#endif
  cout << "total send: " << sended << " packets" << endl;
	if (use_cors)
    cors_sk->close();
	close(sock);

	return 0;
}

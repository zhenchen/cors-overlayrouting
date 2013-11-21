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
#include <list>

#include "../client/corssocket.h"
#include "../utils/utilfunc.h"
#include "jitter.h"

using namespace std;

const int MAXLEN = 2048;
const short DEFAULT_PLAY = 5000;
const short DEFAULT_RECV = 6000;
const uint8_t DEFAULT_RLNUM = 1;
const int DEFAULT_DELAY = 4;
const int DEFAULT_FPS = 30;
const int DEFAULT_NUM = 10000;
const long MAX_TIME = 0xffffffff;

int WriteRtpData(FILE * fp, const timeval & ts,
                 const int len, const uint8_t *pkt)
{
  fwrite(&ts, sizeof(ts), 1, fp);
  fwrite(&len, sizeof(len), 1, fp);
  fwrite(pkt, 1, len, fp);
  return 0;
}

class StreamPlayer {
 public:
  StreamPlayer();
  ~StreamPlayer();
  
  void StartPlay();
  void StopPlay();

  // threads for playing
  static void *PlayThread(void *arg);
  static void *TimerThread(void *arg);
  
  // Main loop for recving incoming stream
  void ListenStream();

 public:
  JitterBuffer jitter_;
  pthread_mutex_t jitter_lock_;
  
  uint32_t timer_;
  pthread_mutex_t timer_lock_;
  pthread_cond_t timer_cond_;

  pthread_t recv_tid_;
  pthread_t play_tid_;
  pthread_t timer_tid_;

  bool running_;
  bool use_cors_;
  std::string save_as_;
  int total_packets_;
  int fps_; // frame per second
  int recv_sock_;
  int play_sock_;
  int play_delay_;  // number of frames of PlayThread delay
  sockaddr_in play_addr_;
  sockaddr_in remote_addr_;
  CorsSocket *cors_;
};

StreamPlayer::StreamPlayer() {
  pthread_mutex_init(&jitter_lock_, NULL);
  pthread_mutex_init(&timer_lock_, NULL);
  pthread_cond_init(&timer_cond_, NULL);

  timer_ = -1;  // wait for signal
  running_ = false;
  use_cors_ = false;
  fps_ = 30;
  recv_sock_ = 0;
  play_sock_ = 0;
  play_delay_ = 3;
  cors_ = NULL;
}

StreamPlayer::~StreamPlayer() {
  StopPlay();
  pthread_mutex_destroy(&jitter_lock_);
  pthread_mutex_destroy(&timer_lock_);
  pthread_cond_destroy(&timer_cond_);
}

void StreamPlayer::StartPlay() {
  if (!running_) {
    running_ = true;
    pthread_create(&play_tid_, NULL, PlayThread, this);
    pthread_create(&timer_tid_, NULL, TimerThread, this);
  }
}

void StreamPlayer::StopPlay() {
  if (running_) {
    running_ = false;
    pthread_join(play_tid_, NULL);
    pthread_join(timer_tid_, NULL);
  }
}

void StreamPlayer::ListenStream() {
  fd_set rfds;
  char rbuf[MAXLEN];
  int rlen = -1;
  sockaddr_in from;
  socklen_t fromlen = sizeof(from);
  timeval timeout;
  int stop = 0;
  bool first = true;
  int recv_packets = 0;

  while (true) {
    FD_ZERO(&rfds);
    FD_SET(recv_sock_, &rfds);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    int ret = select(recv_sock_ + 1, &rfds, NULL, NULL, &timeout);
    if (ret < 0) {
      cerr << "select() error" << endl;
      break;
    } else if (ret == 0) {
      break;
    } else {
      if (use_cors_) {
        rlen = -1;
        cors_->recvfrom(rbuf, rlen, from.sin_addr.s_addr, from.sin_port);
        if (rlen < 0) {
          if (++stop == 20) break;
          continue;
        } else {
          stop = 0;
        }
      } else {
        rlen = recvfrom(recv_sock_, rbuf, MAXLEN, 0, (sockaddr *)&from, &fromlen);
      }
      if (from.sin_addr.s_addr != remote_addr_.sin_addr.s_addr)
        continue;

      RTPDataFrame frame(rlen);
      memcpy(frame.GetPointer(), rbuf, rlen);
      // cout << "recv frame " << frame.GetSequenceNumber()
      //      << " from " << sockaddr2str(from) << endl;
      pthread_mutex_lock(&jitter_lock_);
      jitter_.Write(frame);
      pthread_mutex_unlock(&jitter_lock_);
      recv_packets++;
      if (first) {
        StartPlay();
        first = false;
      }
    }
  }
  cout << "total received packets: " << recv_packets << endl;
}

void *StreamPlayer::PlayThread(void *arg) {
  StreamPlayer *player = static_cast<StreamPlayer *>(arg);
  uint32_t ts_played = 0;
  uint32_t ts_to_play = 0;
  int finished = 0;
  int played_packets = 0;
  timeval timestamp = {0, 0};
  FILE *fp = fopen(player->save_as_.c_str(), "wb");
  if (fp == NULL) {
    cerr << "cannot open file: " << player->save_as_ << endl;
    return NULL;
  }
  while (player->running_) {
    pthread_mutex_lock(&player->timer_lock_);
    while (player->timer_ == -1) {
      pthread_cond_wait(&player->timer_cond_, &player->timer_lock_);
    }
    ts_to_play = player->timer_;
    player->timer_ = -1;
    pthread_mutex_unlock(&player->timer_lock_);
  
    if (ts_to_play == -2) 
      continue; // check running_ to exit thread

    for (uint32_t i = ts_played; i <= ts_to_play; i++) {
      do {
        // cout << "play ts = " << i << endl;
        pthread_mutex_lock(&player->jitter_lock_);
        RTPDataFrame *pframe =
          player->jitter_.Read(i, &finished);
        pthread_mutex_unlock(&player->jitter_lock_);
        
        if (pframe == NULL)
          continue;
        
        // send to MyPlayer
        // cout << "send frame: " << endl;
        // cout << "\tseq = " << pframe->GetSequenceNumber() << endl;
        // cout << "\tts = " << pframe->GetTimestamp() << endl;
        // cout << "\tSSRC = " << pframe->GetSSRC() << endl;
        // cout << "\tmark = " << (pframe->GetMarker() ? "true" : "false") << endl;
        sendto(player->play_sock_, 
               pframe->GetPointer(),
               pframe->GetSize(), 0, 
               (sockaddr *)&player->play_addr_,
               sizeof(player->play_addr_));
        // save to a dump file
        WriteRtpData(fp, timestamp,
                     pframe->GetSize(),
                     pframe->GetPointer());

        played_packets++;
        delete pframe;
      } while (!finished);
      ts_played++;
    }
  }
  cout << "total packets: " << player->total_packets_ << endl;
  cout << "played packets: " << played_packets << endl;
  int lost_packets = player->total_packets_ - played_packets;
  cout << "lost packets: " << lost_packets << endl;
  double lossrate = (double) lost_packets / player->total_packets_;
  fprintf(stdout, "lossrate: %.2f\%\n", lossrate * 100);
  timestamp.tv_sec = timestamp.tv_usec = MAX_TIME;
  fwrite(&timestamp, sizeof(timestamp), 1, fp);
  fclose(fp);
  return NULL;
}

void *StreamPlayer::TimerThread(void *arg) {
  StreamPlayer *player = static_cast<StreamPlayer *>(arg);
  timeval timeout;
  const unsigned long TIMESLOT = 1000000 / player->fps_ + 1;
  uint32_t i = 0;
  // delay in PlayThread for JitterBuffer
  timeout.tv_sec = player->play_delay_ * TIMESLOT / 1000000;
  timeout.tv_usec = player->play_delay_ * TIMESLOT % 1000000;
  select(0, NULL, NULL, NULL, &timeout);
  while (player->running_) {
    pthread_mutex_lock(&player->timer_lock_);
    player->timer_ = i;
    pthread_cond_signal(&player->timer_cond_);
    pthread_mutex_unlock(&player->timer_lock_);
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMESLOT;
    select(0, NULL, NULL, NULL, &timeout);
    i++;
  }
  pthread_mutex_lock(&player->timer_lock_);
  player->timer_ = -2; // termination signal for PlayThread
  pthread_cond_signal(&player->timer_cond_);
  pthread_mutex_unlock(&player->timer_lock_);
  return NULL;
}

void PrintUsage() {
  cerr << "usage: ./streamplayer [option]... <dumpfile>" << endl;
  cerr << "\t-c: use CORS API" << endl;
  cerr << "\t-r recvport: rtp-receiving port number" << endl;
  cerr << "\t-p playport: player port number" << endl;
  cerr << "\t-d playdelay: delay in play thread" << endl;
  cerr << "\t-f fps: play speed, frame per second" << endl;
  cerr << "\t-s ip:port: remote sender's address & port" << endl;
  cerr << "\t-n num: total number of packets (for lossrate)" << endl;
}

timeval CalcDelta(const timeval & tm1, const timeval & tm2)
{
  timeval delta;
  assert(tm1.tv_sec > tm2.tv_sec ||
	       (tm1.tv_sec == tm2.tv_sec && tm1.tv_usec >= tm2.tv_usec));
  if (tm1.tv_usec >= tm2.tv_usec) {
    delta.tv_usec = tm1.tv_usec - tm2.tv_usec;
    delta.tv_sec = tm1.tv_sec - tm2.tv_sec;
  } else {
    delta.tv_usec = 1000000 + tm1.tv_usec - tm2.tv_usec;
    delta.tv_sec = tm1.tv_sec - tm2.tv_sec - 1;
  }
  return delta;
}

int InitSocket(sockaddr_in *bind_addr)
{
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (bind_addr == NULL) 
    return sock;
  
  if (bind(sock, (sockaddr *)bind_addr, sizeof(*bind_addr)) < 0) {
    cerr << "bind() error" << endl;
    close(sock);
    return -1;
  }
  return sock;
}

int main(int argc, char *argv[])
{
  int c;
  bool use_cors = false;
  std::string remote_str;
  std::string save_as_file;
  sockaddr_in recv_addr, play_addr, remote_addr;
  timeval timestamp, tm_start, tm_delta;
  int play_delay = DEFAULT_DELAY;
  int fps = DEFAULT_FPS;
  int total = DEFAULT_NUM; 

  memset(&recv_addr, 0, sizeof(recv_addr));
  recv_addr.sin_family = AF_INET;
  recv_addr.sin_port = htons(DEFAULT_RECV);
  recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  memset(&play_addr, 0, sizeof(play_addr));
  play_addr.sin_family = AF_INET;
  play_addr.sin_port = htons(DEFAULT_PLAY);
  play_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  while ((c = getopt(argc, argv, "cr:p:d:f:s:n:")) != -1) {
    switch (c) {
    case 'c':
      use_cors = true;
      break;
    case 'r':
      recv_addr.sin_port = htons(atoi(optarg));
      break;
    case 'p':
      play_addr.sin_port = htons(atoi(optarg));
      break;
    case 'd':
      play_delay = atoi(optarg); 
      break;
    case 's':
      remote_str = optarg;
      break;
    case 'f':
      fps = atoi(optarg);
      break;
    case 'n':
      total = atoi(optarg);
      break;
    case '?':
      PrintUsage();
      break;
    }
  }
  if (optind != argc - 1) {
    if (use_cors)
      save_as_file = "dump_cors";
    else
      save_as_file = "dump_nocors";
  } else {
    save_as_file = argv[optind];
  }

  if (remote_str.empty()) {
    cout << "must specify remote addr -- ip:port" << endl;
    PrintUsage();
    return -1;
  }
  size_t pos = remote_str.find(":");
  memset(&remote_addr, 0, sizeof(remote_addr));
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(atoi(remote_str.substr(pos + 1).c_str()));
  remote_addr.sin_addr.s_addr = inet_addr(remote_str.substr(0, pos).c_str());

  cout << "use_cors: " << (use_cors ? "yes" : "no") << endl;
  cout << "receive addr: " << sockaddr2str(recv_addr) << endl;
  cout << "player addr: " << sockaddr2str(play_addr) << endl;
  cout << "remote addr: " << sockaddr2str(remote_addr) << endl;
  cout << "save as file: " << save_as_file << endl;

  // FILE *fp = fopen("dump", "wb");
  // if (!fp) {
  //   cerr << "cannot open dump file" << endl;
  //   return -1;
  // }

  int recv_sock = InitSocket(&recv_addr);
  if (recv_sock < 0) return -1;
  
  int play_sock = InitSocket(NULL);

  Requirement req;
  memset(&req, 0, sizeof(req));
  req.delay = 300;
  req.relaynum = 1;

  Feature feature;
  memset(&feature, 0, sizeof(feature));
  feature.delay = 300;
  feature.significance = 0;
  feature.seculevel = 0;
  feature.channelnum = 1;

  vector <string> rsparams, mmparams;
  rsparams.push_back("SimpleRlysltr");
  rsparams.push_back("rsltr.conf");
  rsparams.push_back("500000");
  rsparams.push_back("300");

  mmparams.push_back("SimpleMpathManager");

  CorsSocket cors_sk(recv_sock, rsparams, mmparams);
  cors_sk.init(req, remote_addr.sin_addr.s_addr, remote_addr.sin_port);

  StreamPlayer player;
  player.use_cors_ = use_cors;
  player.fps_ = fps;
  player.recv_sock_ = recv_sock;
  player.play_sock_ = play_sock;
  player.play_delay_ = play_delay;
  player.play_addr_ = play_addr;
  player.remote_addr_ = remote_addr;
  player.cors_ = &cors_sk;
  player.save_as_ = save_as_file;
  player.total_packets_ = total;

  // player.StartPlay();
  player.ListenStream();
  player.StopPlay();

  // fclose(fp);
  cors_sk.close();
  close(recv_sock);
  close(play_sock);
  // cout << "Exit..." << endl;
  return 0;
}

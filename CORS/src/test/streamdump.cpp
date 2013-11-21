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
#include <map>
#include <list>

#include "../client/corssocket.h"
#include "../utils/utilfunc.h"
#include "jitter2.h"

using namespace std;

const int MAXLEN = 2048;
const short DEFAULT_PLAY = 5000;
const short DEFAULT_RECV = 6000;
const uint8_t DEFAULT_RLNUM = 1;
const int DEFAULT_DELAY = 4;
const int DEFAULT_FPS = 30;
const long MAX_TIME = 0xffffffff;

void PrintUsage() {
  cerr << "usage: ./streamdump [option]... file" << endl;
  cerr << "\t-c: use CORS API" << endl;
  cerr << "\t-r recvport: rtp-receiving port number" << endl;
  cerr << "\t-p playport: player port number" << endl;
  cerr << "\t-d playdelay: delay in play thread" << endl;
  cerr << "\t-f fps: play speed, frame per second" << endl;
  cerr << "\t-s ip:port: remote sender's address & port" << endl;
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
  int fps_; // frame per second
  int recv_sock_;
  int play_sock_;
  int play_delay_;  // number of frames of PlayThread delay
  std::string output_file_;
  sockaddr_in play_addr_;
  sockaddr_in remote_addr_;
  CorsSocket *cors_;

  map<uint16_t, RTPDataFrame> frames_;
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
  frames_.clear();
}

StreamPlayer::~StreamPlayer() {
  StopPlay();
  pthread_mutex_destroy(&jitter_lock_);
  pthread_mutex_destroy(&timer_lock_);
  pthread_cond_destroy(&timer_cond_);
  frames_.clear();
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
  int path = 0;

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
        cors_->recvfrom(rbuf, rlen, from.sin_addr.s_addr, from.sin_port, &path);
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
      cout << "recv frame " << frame.GetSequenceNumber()
           << " from " << sockaddr2str(from) 
           << (path == 0 ? " Direct" : " Indirect") << endl;
      frames_.insert(pair<uint16_t, RTPDataFrame>(frame.GetSequenceNumber(), frame));
      // pthread_mutex_lock(&jitter_lock_);
      // jitter_.Write(frame, path);
      // pthread_mutex_unlock(&jitter_lock_);
      recv_packets++;
      // if (first) {
      //   StartPlay();
      //   first = false;
      // }
    }
  }
  FILE *fp = fopen(output_file_.c_str(), "wb");
  if (!fp) {
    cerr << "cannot open output file " << output_file_ << endl;
    return;
  }
  int played_packets = 0;
  map<uint16_t, RTPDataFrame>::iterator it = frames_.begin();
  for (; it != frames_.end(); ++it) {
    timeval tmp = {0, 0};
    WriteRtpData(fp, tmp, it->second.GetSize(), it->second.GetPointer());
    played_packets++;
  }
  timeval tm_fin;
  tm_fin.tv_sec = MAX_TIME;
  tm_fin.tv_usec = MAX_TIME;
  fwrite(&tm_fin, sizeof(tm_fin), 1, fp);
  fclose(fp);
  cout << "total received packets: " << recv_packets << endl;
  cout << "total played packets: " << played_packets << endl;
}

void *StreamPlayer::PlayThread(void *arg) {
  StreamPlayer *player = static_cast<StreamPlayer *>(arg);
  uint32_t ts_played = 0;
  uint32_t ts_to_play = 0;
  int finished = 0;
  int played_packets = 0;
  bool first = true;
  timeval timestamp, tm_start, tm_delta;

  FILE *fp = fopen(player->output_file_.c_str(), "wb");
  if (!fp) {
    cerr << "cannot open output file " << player->output_file_ << endl;
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
  
    if (ts_to_play == -2) { 
      do {
        // clear jitter
        pthread_mutex_lock(&player->jitter_lock_);
        RTPDataFrame *pframe = 
          player->jitter_.Read(0xffffffff, &finished);
        pthread_mutex_unlock(&player->jitter_lock_);

        if (pframe == NULL)
          continue;

        cout << "send frame: " << endl;
        cout << "\tseq = " << pframe->GetSequenceNumber() << endl;
        cout << "\tts = " << pframe->GetTimestamp() << endl;
        cout << "\tmark = " << (pframe->GetMarker() ? "true" : "false") << endl;
        gettimeofday(&timestamp, NULL);
        if (first) {
          tm_start = timestamp;
          tm_delta.tv_sec = 0;
          tm_delta.tv_usec = 0;
          first = false;
        } else {
          tm_delta = CalcDelta(timestamp, tm_start);
        }
        WriteRtpData(fp, tm_delta, pframe->GetSize(), pframe->GetPointer());
        played_packets++;
        delete pframe;
      } while (!finished);
      continue; // check running_ to exit thread
    }

    for (uint32_t i = ts_played; i <= ts_to_play; i++) {
      do {
        cout << "play ts = " << i << endl;
        pthread_mutex_lock(&player->jitter_lock_);
        RTPDataFrame *pframe =
          player->jitter_.Read(0, &finished);
        pthread_mutex_unlock(&player->jitter_lock_);
        
        if (pframe == NULL)
          continue;
        
        // send to MyPlayer
        cout << "send frame: " << endl;
        cout << "\tseq = " << pframe->GetSequenceNumber() << endl;
        cout << "\tts = " << pframe->GetTimestamp() << endl;
        cout << "\tmark = " << (pframe->GetMarker() ? "true" : "false") << endl;
        gettimeofday(&timestamp, NULL);
        if (first) {
          tm_start = timestamp;
          tm_delta.tv_sec = 0;
          tm_delta.tv_usec = 0;
          first = false;
        } else {
          tm_delta = CalcDelta(timestamp, tm_start);
        }
        WriteRtpData(fp, tm_delta, pframe->GetSize(), pframe->GetPointer());
        // sendto(player->play_sock_, 
        //        pframe->GetPointer(),
        //        pframe->GetSize(), 0, 
        //        (sockaddr *)&player->play_addr_,
        //        sizeof(player->play_addr_));
        played_packets++;
        delete pframe;
      } while (!finished);
      ts_played++;
    }
  }
  timeval tm_fin;
  tm_fin.tv_sec = MAX_TIME;
  tm_fin.tv_usec = MAX_TIME;
  fwrite(&tm_fin, sizeof(tm_fin), 1, fp);
  fclose(fp);
  cout << "total played packets: " << played_packets << endl;
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
  sockaddr_in recv_addr, play_addr, remote_addr;
  timeval timestamp, tm_start, tm_delta;
  int play_delay = DEFAULT_DELAY;
  int fps = DEFAULT_FPS;

  memset(&recv_addr, 0, sizeof(recv_addr));
  recv_addr.sin_family = AF_INET;
  recv_addr.sin_port = htons(DEFAULT_RECV);
  recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  memset(&play_addr, 0, sizeof(play_addr));
  play_addr.sin_family = AF_INET;
  play_addr.sin_port = htons(DEFAULT_PLAY);
  play_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  while ((c = getopt(argc, argv, "cr:p:d:f:s:")) != -1) {
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
    case '?':
      PrintUsage();
      break;
    }
  }
  if (optind != argc - 1) {
    cout << "must specify an output file" << endl;
    PrintUsage();
    return -1;
  }
  std::string filename = argv[optind];

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

  FILE *fp = fopen("dump", "wb");
  if (!fp) {
    cerr << "cannot open dump file" << endl;
    return -1;
  }

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
  player.output_file_ = filename;

  // player.StartPlay();
  player.ListenStream();
  // player.StopPlay();

  fclose(fp);
  cors_sk.close();
  close(recv_sock);
  close(play_sock);
  cout << "Exit..." << endl;
  return 0;
}

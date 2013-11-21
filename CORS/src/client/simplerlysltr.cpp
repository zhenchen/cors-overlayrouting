#include <netinet/in.h> // inet_addr()
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>   // gettimeofday()

#include <iostream>
#include <sstream>
#include <fstream>      // ifstream
#include <stdexcept>

#include "multipathmanager.h"
#include "simplerlysltr.h"
#include "../common/corsprotocol.h"
#include "../utils/debug.h"
#include "../utils/ip2asn.h"

using namespace std;
using namespace cors;

SimpleRlysltr::SimpleRlysltr(CorsSocket *sk, std::vector<std::string> &param)
  : RelaySelector(sk), probe_interval_(1), time_out_(300)
{
  config_ = param[CONFIG];
  ifstream ifs(config_.c_str());
  if (!ifs) {
    throw runtime_error("Construct SimpleRlysltr failed");
    return;
  }

  string line;
  // RelayUnit boot;
  EndPoint boot;
  string ip_str, mask_str;
  in_port_t port;
  while (!getline(ifs, line).eof()) {
    istringstream iss(line);
    iss >> ip_str >> mask_str >> port;
    boot.addr = inet_addr(ip_str.c_str());
    boot.mask = inet_addr(mask_str.c_str());
    boot.port = htons(port);
    boot.asn = ip2asn(boot.addr);
    candidates_.push_back(boot);
  }
  ifs.close();
  if (candidates_.empty()) {
    throw runtime_error("Construct SimpleRlysltr failed");
    return;
  }
#if (defined(RSLTR_INTEST))
  int index = 0;
  for (list<EndPoint>::iterator it = candidates_.begin(); it != candidates_.end(); ++it) {
    DEBUG_PRINT(4, "cand(%d): %s\n", index, it->AsString().c_str());
    ++index;
  }
#endif

  vector<string>::size_type n = param.size();

  if (n <= PROBE_INTERVAL) return;
  istringstream iss1(param[PROBE_INTERVAL]);
  iss1 >> probe_interval_;

  if (n <= TIME_OUT) return;
  istringstream iss2(param[TIME_OUT]);
  iss2 >> time_out_;
  
  pthread_mutex_init(&candidates_mutex_, NULL);
  pthread_mutex_init(&relaylist_mutex_, NULL);

  running_ = true;
  iter_candidates = candidates_.begin();
  iter_relaylist = relaylist_.begin();
  //iter_candidates = candidates_.begin();
  //iter_relaylist = relaylist_.begin();

  cout << "creating thread" << endl;
  pthread_create(&cycle_tid_, NULL, cycle_ping, this);

  last_report_time_ = time(NULL);
  // signal(SIGALRM,cycle_ping);
}

SimpleRlysltr::~SimpleRlysltr()
{
  pthread_mutex_destroy(&candidates_mutex_);
  pthread_mutex_destroy(&relaylist_mutex_);

  candidates_.clear();
  relaylist_.clear();
}

void 
SimpleRlysltr::increase_iter() 
{
  pthread_mutex_lock(&candidates_mutex_);
  ++iter_candidates;
  if (iter_candidates == candidates_.end()) 
    iter_candidates = candidates_.begin();
  pthread_mutex_unlock(&candidates_mutex_);

  pthread_mutex_lock(&relaylist_mutex_);
  ++iter_relaylist;
  if (iter_relaylist == relaylist_.end()) 
    iter_relaylist = relaylist_.begin();
  pthread_mutex_unlock(&relaylist_mutex_);

  return;
}

void * 
SimpleRlysltr::cycle_ping(void *pParam)
{
  cout << "cycle_ping thread start..." << endl;

  list<EndPoint>::iterator iter_candidates;
  list<RelayNode>::iterator iter_relaylist;
  sockaddr_in proxyaddr;
  in_port_t cand_port, rly_port;
  in_addr_t cand_addr, rly_addr;
  in_addr_t cand_mask, rly_mask;
  uint16_t cand_asn, rly_asn;
  int len;
  char *msg = NULL;
  SimpleRlysltr *rlysltr = static_cast<SimpleRlysltr *> (pParam);

  usleep(1000);
  // RelayNode direct;
  // direct.addr = rlysltr->psk->get_dstaddr();
  // direct.port = rlysltr->psk->get_dstport();
  // pthread_mutex_lock(&(rlysltr->relaylist_mutex_));
  // rlysltr->relaylist_.clear();
  // rlysltr->relaylist_.push_back(direct);
  // rlysltr->iter_relaylist = rlysltr->relaylist_.begin();
  // pthread_mutex_unlock(&(rlysltr->relaylist_mutex_));
  
  while(true) {
    if(rlysltr->running_ != true) break;
    // scan candidates list
    // candidates should not be empty
    pthread_mutex_lock(&(rlysltr->candidates_mutex_));
    iter_candidates = rlysltr->get_iter_candidates();
    cand_asn = iter_candidates->asn;
    cand_port = iter_candidates->port;
    cand_addr = iter_candidates->addr;
    cand_mask = iter_candidates->mask;
    pthread_mutex_unlock(&(rlysltr->candidates_mutex_));
    
    memset(&proxyaddr, 0, sizeof(proxyaddr));
    proxyaddr.sin_family = AF_INET;
    proxyaddr.sin_port = cand_port;
    proxyaddr.sin_addr.s_addr = cand_addr;
   
    CorsProbe probe;
    probe.type_ = TYPE_FW_PROBE;
    probe.ttl_ = 2;
    probe.source_.port = rlysltr->psk->get_srcport();
    probe.source_.addr = rlysltr->psk->get_srcaddr();
    probe.source_.mask = rlysltr->psk->get_srcmask();
    probe.source_.asn = rlysltr->psk->get_srcasn();

    probe.relay_.port = cand_port;
    probe.relay_.addr = cand_addr;
    probe.relay_.mask = cand_mask;
    probe.relay_.asn = cand_asn;

    probe.destination_.port = rlysltr->psk->get_dstport();
    probe.destination_.addr = rlysltr->psk->get_dstaddr();
    probe.destination_.mask = rlysltr->psk->get_dstmask();
    probe.destination_.asn = rlysltr->psk->get_dstasn();

    gettimeofday(&probe.timestamp_, NULL);
    probe.bandwidth_ = probe.load_num_ = 0;

    int probe_len = 0;
    uint8_t *probe_buf = probe.Serialize(probe_len);

    sendto(rlysltr->psk->get_sock(), probe_buf, probe_len, 0, (sockaddr *)&proxyaddr, sizeof(proxyaddr));
#if (defined(RSLTR_INTEST))
    DEBUG_PRINT(4, "SimpleRlysltr: send probe to relay (%s)\n-->: %s\n%s\n",
                sockaddr2str(proxyaddr).c_str(),
                bytes2str(probe_buf, probe_len).c_str(),
                probe.ParseToString().c_str());
#endif
    delete[] probe_buf;

    time_t now = time(NULL);
    if (now - rlysltr->last_report_time_ > 10) {
      CorsReport report;
      report.source_.asn = rlysltr->psk->get_srcasn();
      report.source_.port = rlysltr->psk->get_srcport();
      report.source_.addr = rlysltr->psk->get_srcaddr();
      report.source_.mask = rlysltr->psk->get_srcmask();

      report.destination_.asn = rlysltr->psk->get_dstasn();
      report.destination_.port = rlysltr->psk->get_dstport();
      report.destination_.addr = rlysltr->psk->get_dstaddr();
      report.destination_.mask = rlysltr->psk->get_dstmask();

      pthread_mutex_lock(&(rlysltr->relaylist_mutex_));
      if (rlysltr->relaylist_.size() > 0) {
        list<RelayNode>::iterator it = rlysltr->relaylist_.begin();
        for (; it != rlysltr->relaylist_.end(); ++it) {
          ReportEntry entry;
          entry.relay = it->relay;
          entry.delay.tv_sec = it->delay / 1000000;
          entry.delay.tv_usec = it->delay % 1000000;
          report.reports_.push_back(entry);
        }
      }
      pthread_mutex_unlock(&(rlysltr->relaylist_mutex_));
      
      if (report.reports_.size() > 0) {
        int report_len = 0;
        uint8_t *report_buf = report.Serialize(report_len);

        sendto(rlysltr->psk->get_sock(), report_buf, report_len, 0, (sockaddr *)&proxyaddr, sizeof(proxyaddr));
#if (defined(RSLTR_INTEST))
        DEBUG_PRINT(4, "SimpleRlysltr: send report to proxy (%s)\n-->: %s\n%s\n",
                    sockaddr2str(proxyaddr).c_str(),
                    bytes2str(report_buf, report_len).c_str(),
                    report.ParseToString().c_str());
#endif
        delete[] report_buf;
      }
      rlysltr->last_report_time_ = now;
    }

    //scan relaylist list
    pthread_mutex_lock(&(rlysltr->relaylist_mutex_));
    iter_relaylist = rlysltr->get_iter_relaylist();
    if (iter_relaylist == rlysltr->relaylist_.end()) {
      pthread_mutex_unlock(&(rlysltr->relaylist_mutex_));
      rlysltr->increase_iter();
      // sleep(rlysltr->probe_interval_);
      usleep(rlysltr->probe_interval_);
      continue;
    }

    rly_port = iter_relaylist->relay.port;
    rly_addr = iter_relaylist->relay.addr;
    rly_mask = iter_relaylist->relay.mask;
    rly_asn = iter_relaylist->relay.asn;
    pthread_mutex_unlock(&(rlysltr->relaylist_mutex_));

    if (rly_addr == cand_addr) {
      rlysltr->increase_iter();
      // sleep(rlysltr->probe_interval_);
      usleep(rlysltr->probe_interval_);
      continue;
    }

    memset(&proxyaddr, 0, sizeof(proxyaddr));
    proxyaddr.sin_family = AF_INET;
    proxyaddr.sin_port = rly_port;
    proxyaddr.sin_addr.s_addr = rly_addr;

    memset(&probe, 0, sizeof(probe));
    probe.type_ = TYPE_FW_PROBE;
    probe.ttl_ = 2;
    probe.source_.port = rlysltr->psk->get_srcport();
    probe.source_.addr = rlysltr->psk->get_srcaddr();
    probe.source_.mask = rlysltr->psk->get_srcmask();
    probe.source_.asn = rlysltr->psk->get_srcasn();

    probe.relay_.port = rly_port;
    probe.relay_.addr = rly_addr;
    probe.relay_.mask = rly_mask;
    probe.relay_.asn = rly_asn;

    probe.destination_.port = rlysltr->psk->get_dstport();
    probe.destination_.addr = rlysltr->psk->get_dstaddr();
    probe.destination_.mask = rlysltr->psk->get_dstmask();
    probe.destination_.asn = rlysltr->psk->get_dstasn();

    gettimeofday(&probe.timestamp_, NULL);
    probe.bandwidth_ = probe.load_num_ = 0;

    probe_buf = probe.Serialize(probe_len);

    sendto(rlysltr->psk->get_sock(), probe_buf, probe_len, 0, (sockaddr *)&proxyaddr, sizeof(proxyaddr));

#if (defined(RSLTR_INTEST))
    DEBUG_PRINT(4, "SimpleRlysltr: send probe to relay (%s)\n-->: %s\n%s\n",
                sockaddr2str(proxyaddr).c_str(),
                bytes2str(probe_buf, probe_len).c_str(),
                probe.ParseToString().c_str());
#endif
    delete[] probe_buf;

    rlysltr->increase_iter();
    usleep(rlysltr->probe_interval_);
  }
  return NULL;
}

void
SimpleRlysltr::get_relaylist(list<RelayNode> &rn_list) {
  pthread_mutex_lock(&relaylist_mutex_);
  rn_list.assign(relaylist_.begin(), relaylist_.end());
  pthread_mutex_unlock(&relaylist_mutex_);
}


void
SimpleRlysltr::send_rl_inquiry(const Requirement &rq, int sk)
{
  pthread_mutex_lock(&candidates_mutex_);
  list<EndPoint>::iterator it = candidates_.begin();
  uint16_t prx_asn = it->asn;
  in_port_t prx_port = it->port;
  in_addr_t prx_addr = it->addr;
  in_addr_t prx_mask = it->mask;
  pthread_mutex_unlock(&candidates_mutex_);

  sockaddr_in proxyaddr;
  memset(&proxyaddr, 0, sizeof(proxyaddr));
  proxyaddr.sin_family = AF_INET;
  proxyaddr.sin_port = prx_port;
  proxyaddr.sin_addr.s_addr = prx_addr;

  CorsInquiry inquiry;
  inquiry.required_relays_ = rq.relaynum * 6;
  inquiry.source_.asn = psk->get_srcasn();
  inquiry.source_.port = psk->get_srcport();
  inquiry.source_.addr = psk->get_srcaddr();
  inquiry.source_.mask = psk->get_srcmask();

  inquiry.destination_.asn = psk->get_dstasn();
  inquiry.destination_.port = psk->get_dstport();
  inquiry.destination_.addr = psk->get_dstaddr();
  inquiry.destination_.mask = psk->get_dstmask();

  inquiry.proxy_.asn = prx_asn;
  inquiry.proxy_.port = prx_port;
  inquiry.proxy_.addr = prx_addr;
  inquiry.proxy_.mask = prx_mask;

  int inquiry_len = 0;
  uint8_t *inquiry_buf = inquiry.Serialize(inquiry_len);

  sendto(sk, inquiry_buf, inquiry_len, 0, (sockaddr *)&proxyaddr, sizeof(proxyaddr));
#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "SimpleRlysltr::send_rl_inquiry(): to proxy (%s)\n-->: %s\n%s\n",
              sockaddr2str(proxyaddr).c_str(),
              bytes2str(inquiry_buf, inquiry_len).c_str(),
              inquiry.ParseToString().c_str());
#endif
  delete[] inquiry_buf;
}

void
SimpleRlysltr::process_rl_advice(char *msg, int len, sockaddr_in &peeraddr, int sk)
{
#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "SimpleRlysltr::process_rl_advice(): receive advice from (%s)\n<--: %S\n",
              sockaddr2str(peeraddr).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif

  CorsAdvice advice;
  if (advice.Deserialize((uint8_t *)msg, len) < 0) {
    DEBUG_PRINT(1, "SimpleRlysltr::process_rl_advice(): Deserialize() error\n");
    return;
  }

#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "%s\n", advice.ParseToString().c_str());
#endif

  if (advice.relays_.size() == 0) {
    DEBUG_PRINT(1, "SimpleRlysltr::process_rl_advice(): advice contains 0 relay\n");
    return;
  }
  // move all elements in rl_list to candidates' tail
  pthread_mutex_lock(&candidates_mutex_);
  if (candidates_.size() + advice.relays_.size() > MAX_CAND_LEN) {
    candidates_.clear();
  }
  candidates_.splice(candidates_.begin(), advice.relays_);
  iter_candidates = candidates_.begin();

#if (defined(RSLTR_INTEST))
  for (list<EndPoint>::iterator it = candidates_.begin();
       it != candidates_.end(); ++it) {
    DEBUG_PRINT(4, "%s\n", it->AsString().c_str());
  }
#endif

  pthread_mutex_unlock(&candidates_mutex_);
}

void
SimpleRlysltr::process_fw_forward(char *msg, int len, sockaddr_in &peeraddr, int sk)
{
#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "SimpleRlysltr::process_fw_forward(): receive fw_forward from (%s)\n<--: %S\n",
              sockaddr2str(peeraddr).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif
  
  CorsProbe probe;
  if (probe.Deserialize((uint8_t *)msg, len) < 0) {
    DEBUG_PRINT(1, "SimpleRlysltr::process_fw_forward(): Deserialize() error\n");
    return;
  }

#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "%s\n", probe.ParseToString().c_str());
#endif

  sockaddr_in srcaddr;
  srcaddr.sin_family = AF_INET;
  srcaddr.sin_port = probe.source_.port;
  srcaddr.sin_addr.s_addr = probe.source_.addr;

  probe.type_ = TYPE_FW_REPLY;
  probe.ttl_--;

  in_addr_t my_mask = psk->get_srcmask();
  if (probe.destination_.mask != my_mask) {
    probe.destination_.mask = my_mask;
  }
  uint16_t my_asn = psk->get_srcasn();
  if (probe.destination_.asn != my_asn) {
    probe.destination_.asn = my_asn;
  }

  int probe_len = 0;
  uint8_t *probe_buf = probe.Serialize(probe_len);

  sendto(sk, probe_buf, probe_len, 0, (sockaddr *)&srcaddr, sizeof(srcaddr));

#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "SimpleRlysltr::process_fw_forward(): send fw_relpy to (%s)\n-->: %s\n%s\n",
              sockaddr2str(srcaddr).c_str(),
              bytes2str(probe_buf, probe_len).c_str(),
              probe.ParseToString().c_str());
#endif

  delete[] probe_buf;
  return;
}

void
SimpleRlysltr::process_bw_forward(char *msg, int len, sockaddr_in &peeraddr, int sk)
{
#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "SimpleRlysltr::process_bw_forward(): receive bw_forward from (%s)\n<--: %S\n",
              sockaddr2str(peeraddr).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif
  
  CorsProbe probe;
  if (probe.Deserialize((uint8_t *)msg, len) < 0) {
    DEBUG_PRINT(1, "SimpleRlysltr::process_bw_forward(): Deserialize() error\n");
    return;
  }

#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "%s\n", probe.ParseToString().c_str());
#endif

  sockaddr_in srcaddr;
  srcaddr.sin_family = AF_INET;
  srcaddr.sin_port = probe.source_.port;
  srcaddr.sin_addr.s_addr = probe.source_.addr;

  probe.type_ = TYPE_BW_REPLY;
  probe.ttl_--;

  in_addr_t my_mask = psk->get_srcmask();
  if (probe.destination_.mask != my_mask) {
    probe.destination_.mask = my_mask;
  }
  uint16_t my_asn = psk->get_srcasn();
  if (probe.destination_.asn != my_asn) {
    probe.destination_.asn = my_asn;
  }

  int probe_len = 0;
  uint8_t *probe_buf = probe.Serialize(probe_len);

  sendto(sk, probe_buf, probe_len, 0, (sockaddr *)&srcaddr, sizeof(srcaddr));

#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "SimpleRlysltr::process_bw_forward(): send bw_relpy to (%s)\n-->: %s\n%s\n",
              sockaddr2str(srcaddr).c_str(),
              bytes2str(probe_buf, probe_len).c_str(),
              probe.ParseToString().c_str());
#endif

  delete[] probe_buf;
  return;
}

void
SimpleRlysltr::process_fw_reply(char *msg, int len, sockaddr_in &peeraddr, int sk)
{
#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "SimpleRlysltr::process_fw_reply(): receive fw_reply from (%s)\n<--: %S\n",
              sockaddr2str(peeraddr).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif

  CorsProbe probe;
  if (probe.Deserialize((uint8_t *)msg, len) < 0) {
    DEBUG_PRINT(1, "SimpleRlysltr::process_fw_reply(): Deserialize() error\n");
    return;
  }

  timeval tm_now, tm_delay;
  gettimeofday(&tm_now, NULL);

#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "%s\n", probe.ParseToString().c_str());
#endif

  if (psk->get_dstmask() != probe.destination_.mask) {
    psk->set_dstmask(probe.destination_.mask);
  }

  if (psk->get_dstasn() != probe.destination_.asn) {
    psk->set_dstasn(probe.destination_.asn);
  }

  if (tm_now.tv_usec < probe.timestamp_.tv_usec) {
    tm_delay.tv_sec = tm_now.tv_sec - probe.timestamp_.tv_sec - 1;
    tm_delay.tv_usec = 1000000 + tm_now.tv_usec - probe.timestamp_.tv_usec;
  } else {
    tm_delay.tv_sec = tm_now.tv_sec - probe.timestamp_.tv_sec;
    tm_delay.tv_usec = tm_now.tv_usec - probe.timestamp_.tv_usec;
  }

  if (probe.relay_.addr == 0) {
    // direct path probe
    // pthread_mutex_lock(&relaylist_mutex_);
    // list<RelayNode>::iterator iter = relaylist_.begin();
    // for (; iter != relaylist_.end(); ++iter) {
    //   if (iter->addr == psk->get_dstaddr()) break;
    // }
    // // iter point to dst
    // iter->delay = ((uint64_t)tm_delay.tv_sec * 1000000 + tm_delay.tv_usec);
    // pthread_mutex_unlock(&relaylist_mutex_);
    return;
  }

  if (meet_requirement(tm_delay)) {
    // send bw_reserve packet to the relay node
    bool in_use = false;
    pthread_mutex_lock(&relaylist_mutex_);
    DEBUG_PRINT(4, "relaylist_.size = %d\n", relaylist_.size());
    // list<RelayNode>::iterator iter = relaylist_.begin();
    // for (; iter != relaylist_.end(); ++iter) {
    //   sockaddr_in tmp;
    //   tmp.sin_family = AF_INET;
    //   tmp.sin_addr.s_addr = iter->addr;
    //   tmp.sin_port = iter->port;
    //   cout << sockaddr2str(tmp) << endl;
    // }

    list<RelayNode>::iterator it = relaylist_.begin();
    for (; it != relaylist_.end(); ++it) {
      if (it->relay.addr == probe.relay_.addr)  {
        //cout << "hit break" << endl;
        in_use = true;
        break;
      }
    }
    pthread_mutex_unlock(&relaylist_mutex_);
    
    if (!in_use && probe.load_num_ < 4) {
      probe.type_ = TYPE_BW_RESERVE;
      probe.ttl_ = 2;
      probe.bandwidth_ = req.bandwidth;

      int probe_len;
      uint8_t *probe_buf = probe.Serialize(probe_len);

      sockaddr_in rlyaddr;
      rlyaddr.sin_family = AF_INET;
      rlyaddr.sin_port = probe.relay_.port;
      rlyaddr.sin_addr.s_addr = probe.relay_.addr;

      sendto(sk, probe_buf, probe_len, 0, (sockaddr *)&rlyaddr, sizeof(rlyaddr));
#if (defined(RSLTR_INTEST))
      DEBUG_PRINT(4, "SimpleRlysltr::process_fw_reply: send bw_reserve to (%s)\n-->: %s\n%s\n",
                sockaddr2str(rlyaddr).c_str(),
                bytes2str(probe_buf, probe_len).c_str(),
                probe.ParseToString().c_str());
#endif
      delete[] probe_buf;
    }
  } else {
    pthread_mutex_lock(&relaylist_mutex_);
    list<RelayNode>::iterator it = relaylist_.begin();
    for (; it != relaylist_.end(); ++it) {
      if (it->relay.addr == probe.relay_.addr) break;
    }
    if (it != relaylist_.end()) {
      DEBUG_PRINT(3, "remove (%s) from relaylist_\n", it->relay.AsString().c_str());
      if (it == iter_relaylist) {
        ++iter_relaylist;
      }
      relaylist_.erase(it);
      if (iter_relaylist == relaylist_.end())
        iter_relaylist = relaylist_.begin();
    }
    pthread_mutex_unlock(&relaylist_mutex_);
  }
}

void
SimpleRlysltr::process_bw_reply(char *msg, int len, sockaddr_in &peeraddr, int sk)
{
#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "SimpleRlysltr::process_bw_reply(): receive bw_reply from (%s)\n<--: %S\n",
              sockaddr2str(peeraddr).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif

  CorsProbe probe;
  if (probe.Deserialize((uint8_t *)msg, len) < 0) {
    DEBUG_PRINT(1, "SimpleRlysltr::process_bw_reply(): Deserialize() error\n");
    return;
  }
  timeval tm_now;
  gettimeofday(&tm_now, NULL);

#if (defined(RSLTR_INTEST))
  DEBUG_PRINT(4, "%s\n", probe.ParseToString().c_str());
#endif

  timeval tm_delay;
  if (tm_now.tv_usec < probe.timestamp_.tv_usec) {
    tm_delay.tv_sec = tm_now.tv_sec - probe.timestamp_.tv_sec - 1;
    tm_delay.tv_usec = 1000000 + tm_now.tv_usec - probe.timestamp_.tv_usec;
  } else {
    tm_delay.tv_sec = tm_now.tv_sec - probe.timestamp_.tv_sec;
    tm_delay.tv_usec = tm_now.tv_usec - probe.timestamp_.tv_usec; 
  }
  uint64_t delay64 = tm_delay.tv_sec * 1000000ULL + tm_delay.tv_usec;
  
  RelayNode relay_node;
  relay_node.relay.asn = probe.relay_.asn;
  relay_node.relay.port = probe.relay_.port;
  relay_node.relay.addr = probe.relay_.addr;
  relay_node.relay.mask = probe.relay_.mask;
  relay_node.delay = delay64;
  relay_node.bandwidth = probe.bandwidth_;

  pthread_mutex_lock(&relaylist_mutex_);
  list<RelayNode>::iterator it = relaylist_.begin();
  int i = 0;
  bool inserted = false;
  for (; i < req.relaynum && it != relaylist_.end(); ++i, ++it) {
    if (it->delay < relay_node.delay) continue;
    else {
      relaylist_.insert(it, relay_node);
      inserted = true;
      break;
    }
  }
  if (i < req.relaynum && !inserted) {
    relaylist_.push_back(relay_node);
  }
  if (relaylist_.size() > req.relaynum) {
    if (iter_relaylist == --relaylist_.end()) {
      iter_relaylist = relaylist_.begin();
    }
    relaylist_.erase(--relaylist_.end());
  }
  pthread_mutex_unlock(&relaylist_mutex_);
}

void
SimpleRlysltr::release_resources(int sk)
{
  running_ = false;
  pthread_join(cycle_tid_, NULL);

  CorsProbe release;
  release.type_ = TYPE_BW_RELEASE;
  release.source_.asn = psk->get_srcasn();
  release.source_.port = psk->get_srcport();
  release.source_.addr = psk->get_srcaddr();
  release.source_.mask = psk->get_srcmask();

  release.destination_.asn = psk->get_dstasn();
  release.destination_.port = psk->get_dstport();
  release.destination_.addr = psk->get_dstaddr();
  release.destination_.mask = psk->get_dstmask();

  sockaddr_in proxyaddr;
  proxyaddr.sin_family = AF_INET;
  // release all preserved resources
  list<RelayNode>::iterator it = relaylist_.begin();
  for (; it != relaylist_.end(); ++it) {
    proxyaddr.sin_port = it->relay.port;
    proxyaddr.sin_addr.s_addr = it->relay.addr;
 
    release.relay_.asn = it->relay.asn;
    release.relay_.port = it->relay.port;
    release.relay_.addr = it->relay.addr;
    release.relay_.mask = it->relay.mask;

    int release_len = 0;
    uint8_t *release_buf = release.Serialize(release_len);

    sendto(sk, release_buf, release_len, 0, (sockaddr *)&proxyaddr, sizeof(proxyaddr));
    delete[] release_buf;
  }
}
  
bool
SimpleRlysltr::meet_requirement(const timeval &tm_delay)
{
  // return true;
  // if (delay < req.delay) return true;
  uint32_t delay = tm_delay.tv_sec * 1000 + tm_delay.tv_usec / 1000;
  DEBUG_PRINT(4, "delay = %d\n", delay);
  if (delay <= req.delay * 1.5) {
    return true;
  } else {
    return false;
  }
}


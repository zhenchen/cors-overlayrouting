// Copyright (C) 2007 Li Tang <tangli99@gmail.com>
//  
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//  
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//  
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//  

#ifndef __PROXY_ADVANCERLDVR_H
#define __PROXY_ADVANCERLDVR_H

#include <vector>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <list>
#include <set>
#include <map>
#include <ext/hash_map>
#include "relayadvisor.h"
#include "../common/corsprotocol.h"
#include "eventqueue.h"

using namespace std;
using namespace __gnu_cxx;
using namespace cors;

/*
// net address pair
struct ANetPair {
  in_addr_t src_net;
  in_addr_t dst_net;
  NetPair(in_addr_t src = 0, in_addr_t dst = 0): src_net(src), dst_net(dst) {}
};

// set of relay network addr
struct Record {
  time_t update;
  NetSet filter;
};

struct HashFunc {
  size_t operator() (const NetPair & key) const {
    size_t __h = 0;
    uint8_t buf[2 * sizeof(in_addr_t)];
    memcpy(buf, &key.src_net, sizeof(in_addr_t));
    memcpy(buf + sizeof(in_addr_t), &key.dst_net, sizeof(in_addr_t));
    for (int i = 0; i < 2 * sizeof(in_addr_t); i++) {
      __h = 5 * __h + buf[i];
    }
    return __h;
  }
};

struct CompFunc {
  bool operator() (const NetPair & k1, const NetPair & k2) const {
    return ((k1.src_net == k2.src_net) && (k1.dst_net == k2.dst_net));
  }
};

typedef hash_map < const NetPair, Record, HashFunc, CompFunc > NetHMap;
//typedef set<in_addr_t> NetSet;
*/

class AdvanceRldvr : public RelayAdvisor {
 public:
  // typedefs for knowledge cache based on AS number
  struct AsnPair {
    uint16_t src_asn;
    uint16_t dst_asn;
    AsnPair(uint16_t s = 0, uint16_t d = 0)
      : src_asn(s), dst_asn(d) {}
  };

  typedef set<uint16_t> AsnSet;
  struct Entry {
    time_t timestamp;
    AsnSet asn_set;
  };

  struct HashFunc {
    size_t operator() (const AsnPair &key) const {
      size_t h = 0;
      uint8_t buf[sizeof(key)];
      memcpy(buf, &key, sizeof(key));
      for (int i = 0; i < sizeof(key); i++) {
        h = 5 * h + buf[i];
      }
      return h;
    }
  };

  struct CompareFunc {
    bool operator() (const AsnPair &k1, const AsnPair &k2) const {
      return (k1.src_asn == k2.src_asn) && 
             (k1.dst_asn == k2.dst_asn);
    }
  };

  typedef hash_map<AsnPair, Entry, HashFunc, CompareFunc> AsnHashMap;

 public:
  // inner classes
  class Task {
   public:
    Task(const CorsInquiry &inquiry)
      : inquiry_(inquiry) {
      gettimeofday(&timestamp_, NULL);
    }
    ~Task() {}

   public:
    timeval timestamp_;
    CorsInquiry inquiry_;
  };

  class UpdateEvent : public Event {
   public: 
    UpdateEvent(Time t, RelayAdvisor * r, 
                NeighborMaintainer * n, int s)
      : Event(t), pRldvr(r), pNbmtr(n), sk(s) {}

    virtual void execute() {
      pRldvr->update(sk, pNbmtr);
      EventQueue::Instance()->add_event(new UpdateEvent(time(NULL) + pRldvr->get_update_interval(), pRldvr, pNbmtr, sk));
    }

   private:
    RelayAdvisor * pRldvr;
    NeighborMaintainer * pNbmtr;
    int sk;
  };

 public:
  enum PARAM_INDEX {
    DATABASE_ADDR,
    DATABASE_PORT
  };

  AdvanceRldvr(const vector<string> &param);
  ~AdvanceRldvr() {}

  bool initialize(const vector<string> &param);

  virtual void start(int sk, NeighborMaintainer * nbmtr);
  virtual void update(int sk, NeighborMaintainer * nbmtr);
  virtual time_t get_update_interval() { return 1; }

  virtual void process_rl_inquiry(char *msg, int len, sockaddr_in & peeraddr,
                                  int sk, NeighborMaintainer * nbmtr);
  virtual void process_rl_report(char *msg, int len, sockaddr_in & peeraddr,
                                 int sk);
  virtual void process_rl_filter(char *msg, int len, sockaddr_in & peeraddr,
                                 int sk, NeighborMaintainer * nbmtr);

 private:
  pthread_mutex_t task_queue_lock;
  list<Task> task_queue;

  AsnHashMap asn_cache;

  in_addr_t db_addr;
  in_port_t db_port;

  char buff[cors::CORS_MAX_MSG_SIZE];
};

#endif

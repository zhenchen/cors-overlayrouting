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

#ifndef __PROXY_SIMPLENBMTR_H
#define __PROXY_SIMPLENBMTR_H

#include <vector>
#include <map>
#include <list>
#include <string>
#include <time.h>
#include "neighbormaintainer.h"
#include "../common/corsprotocol.h"
#include "eventqueue.h"
#include "neighbor.h"

#if 0
struct Neighbor {
    enum NB_TYPE { ORACLE, PEER };
    Neighbor(char ty=PEER, in_addr_t a=0, in_port_t p=0, time_t t=0,in_addr_t m=0xFFFFFFFF, uint16_t as=0, double r=0, uint8_t s=0) : type(ty), addr(a), port(p), ts(t), mask(m), asn(as), rtt(r), seq(s) {}
    char type;
    in_addr_t addr;
    in_port_t port;
    time_t ts;
    in_addr_t mask;
    uint16_t asn;    
    double rtt;
    uint8_t seq;
};
#endif

class SimpleNbmtr : public NeighborMaintainer {
 public:
  
   typedef struct NetInterface {
    NetInterface(const std::string & na="", in_addr_t ad=0, in_addr_t ma=0, in_port_t po=0, short mt=0)
	    : name(na), addr(ad), mask(ma), port(po), mtu(mt), asn(0) {}
    
    std::string name;
    in_addr_t addr, mask;
    in_port_t port;
    short mtu;
    uint16_t asn;
  }NetInterface;
    
  class ProbeEvent : public Event {
   public:
    ProbeEvent(Time t, NeighborMaintainer * p, int s) 
      : Event(t), pNbmtr(p), sk(s) {}
     
    virtual void execute() {
      pNbmtr->probe(sk);
      EventQueue::Instance()->add_event(new ProbeEvent(time(NULL) + pNbmtr->get_probe_interval(), pNbmtr, sk));
    }

   private:
    NeighborMaintainer * pNbmtr;
    int sk;
  };

  class SaveEvent : public Event {
   public:
    SaveEvent(Time t, NeighborMaintainer * p)
      : Event(t), pNbmtr(p) {}

    virtual void execute() {
      pNbmtr->save();
      EventQueue::Instance()->add_event(new SaveEvent(time(NULL) + pNbmtr->get_save_interval(), pNbmtr));
    }
   private:
    NeighborMaintainer * pNbmtr;
  };
    
  enum PARAM_INDEX { CONF_FILE, PROBE_INTERVAL, SAVE_INTERVAL, MIN_SIZE, MAX_SIZE, NUMBER_PER_QUERY, ABATE_INTERVAL, PARAM_NUM };
  enum CONF_LINE_INDEX { TYPE, ADDR, PORT, RTT, TS, CONF_LINE_PARAM_NUM};
  SimpleNbmtr(std::vector<std::string> & param);
  virtual void start(int sk);
  virtual time_t get_save_interval() { return save_interval; }
  virtual time_t get_probe_interval() { return probe_interval; }
  virtual bool save();
  virtual bool probe(int sk);
  virtual void process_probe_ping(char * msg, int len, sockaddr_in & peeraddr, int sk);
  virtual void process_probe_pong(char * msg, int len, sockaddr_in & peeraddr, int sk);
  virtual int fetch_candidate(int num, time_t update, const NetSet &filter, RluList &rl_list);
  virtual int get_neighbor_num() { return nl.size(); }
  virtual int get_neighbor(int index, RelayUnit &rl_unit);
  virtual int fetch_neighbor_list(NList &nb_list);
  virtual NList *get_neighbor_list() { return &nl; }

 private:
  void get_local_interface();
  bool isself(in_addr_t addr);
    
  std::string conf_file;
  time_t probe_interval;
  time_t save_interval;
  time_t abate_interval;
  int minsize;
  int maxsize;
  char numperquery;

  // typedef std::map<in_addr_t, Neighbor*> NMap;
  // typedef std::list<Neighbor*> NList;
  NMap nm;
  NList nl;
    
  char buff[cors::CORS_MAX_MSG_SIZE];

  std::vector<NetInterface> ifvec;

  //packet format
  // packet type: 1 byte
  // timestamp: 8 bytes
  // #neighbors: 1 byte
  // seq: 1 byte
  // probed neighbor's addr: 4 bytes
  // probed neighbor's mask: 4 bytes (only in pong msg)
  // probed neighbor's asn: 2 bytes (only in pong msg)
  // list of neighbors: 13 bytes per element: 1 byte type, 4 bytes addr, 2 bytes port, 4 bytes mask, 2 bytes asn  (only in pong msg)
  static const int TYPE_POS = 0;
  static const int TYPE_LEN = 1;
  static const int TS_POS = TYPE_POS + TYPE_LEN;
  static const int TS_LEN = sizeof(timeval);
  static const int NBNUM_POS = TS_POS + TS_LEN;
  static const int NBNUM_LEN = 1;
  static const int SEQ_POS = NBNUM_POS + NBNUM_LEN;
  static const int SEQ_LEN = 1;
  static const int ADDR_POS = SEQ_POS + SEQ_LEN;
  static const int ADDR_LEN = sizeof(in_addr_t);
  static const int PING_HEADER_LEN = ADDR_POS + ADDR_LEN;
  static const int MASK_POS = ADDR_POS + ADDR_LEN;
  static const int MASK_LEN = sizeof(in_addr_t);
  static const int ASN_POS = MASK_POS + MASK_LEN;
  static const int ASN_LEN = sizeof(uint16_t);
  static const int PONG_HEADER_LEN = ASN_POS + ASN_LEN;
  static const int PORT_LEN = sizeof(in_port_t);
};

#endif

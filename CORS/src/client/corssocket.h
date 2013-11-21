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

#ifndef __CLIENT_CORSSOCKET_H
#define __CLIENT_CORSSOCKET_H

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include "types.h"
#include "corsprotocol.h"

/** Requirement of overlay path */
typedef struct Requirement {
  uint32_t delay;
  uint32_t bandwidth;
  uint32_t lossrate;
  uint8_t qospara;
  uint8_t seculevel;
  uint8_t relaynum;
} Requirement;

/** Feature of packet */
typedef struct Feature {
  uint32_t delay;
  /** inportance level */
  uint8_t significance;
  /** security level */
  uint8_t seculevel;
  uint8_t channelnum;
} Feature;

/** information of relay node */
typedef struct RelayNode {
  /** address information */
  cors::EndPoint relay;
  // in_addr_t addr;
  // in_port_t port;
  /** TRTT delay */
  uint64_t delay;
  /** lossrate */
  uint32_t lossrate;
  /** available bandwidth */
  uint32_t bandwidth;    
} RelayNode;

/** information of a local network interface */
typedef struct NetInterface {
  std::string name;
  in_addr_t addr;
  in_addr_t mask;
  in_port_t port;
  short mtu;
  uint16_t asn;
  NetInterface(const std::string &na = "", in_addr_t ad = 0, in_addr_t ma = 0, in_port_t po = 0, short mt = 0)
    : name(na), addr(ad), mask(ma), port(po), mtu(mt), asn(0) {}
} NetInterface;

class RelaySelector;
class MultipathManager;

/** client API encapsulation */
class CorsSocket {
 public:
  /** constructor, call RsltrFactory / MmgrFactory to produce the RelaySelector / MultipathManager
   * \param sk descriptor of a socket, must be created and binded already before
   * \param rsparams parameters of RelaySelector, a string vector
   * \param mmparams parameters of MultipathManager, a string vector
   */
  CorsSocket(int sk, std::vector<std::string> & rsparams, std::vector<std::string> & mmparams);
  ~CorsSocket();
  void init(const Requirement & rq, in_addr_t daddr, in_port_t dport); 
  int sendto(char * buff, int nbytes, in_addr_t dstaddr, in_port_t dstport, const Feature & feature);
  int recvfrom(char * buff, int & nbytes, in_addr_t & saddr, in_port_t & sport, int *flag = NULL);
  void close();

  /** \return source ip address */
  in_addr_t get_srcaddr() { return srcaddr; }
  /** \return source ip mask */
  in_addr_t get_srcmask() { return srcmask; }
  /** \return source port */
  in_port_t get_srcport() { return srcport; }
  /** \return source as number */
  uint16_t get_srcasn() { return srcasn; }

  /** \return destination ip address */
  in_addr_t get_dstaddr() { return dstaddr; }
  /** \return destination ip mask */
  in_addr_t get_dstmask() { return dstmask; }
  /** \return destination port */
  in_port_t get_dstport() { return dstport; }
  /** \return destination as number */
  uint16_t get_dstasn() { return dstasn; }

  /** set destination ip mask */
  void set_dstmask(in_addr_t mask) { dstmask = mask; }
  /** set destination as number */
  void set_dstasn(uint16_t asn) { dstasn = asn; }

  /** \return pointer to a RelaySelector object */
  RelaySelector *get_rlysltr() { return rlysltr; }
  /** \return pointer to a MultipathManager object */
  MultipathManager *get_mpthmgr() { return mpthmgr; }
  /** \return file descriptor of the underlying udp socket */
  int get_sock() { return sock; }

 private:
  /** get information of local network interface, e.g. lo & eth0... */
  void get_local_interface();
  /** calculate default mask according to ip */  
  in_addr_t get_default_mask(in_addr_t ip);
  /** look up local as number 
   * \param resolveconf path to config file: resolve.conf
   * */
  void get_self_asn(const std::string & resolveconf = "/etc/resolve.conf");
  
  /** file descriptor of underlying socket */
  int sock;
  /** pointer to a RelaySelector object */
  RelaySelector * rlysltr;
  /** pointer to a MultipathManager object */
  MultipathManager * mpthmgr;
  /** source ip address */
  in_addr_t srcaddr;
  /** source ip mask */
  in_addr_t srcmask;
  /** source port */
  in_port_t srcport;
  /** source as number */
  uint16_t srcasn;
  /** destination ip address */
  in_addr_t dstaddr;
  /** destination ip mask */
  in_addr_t dstmask;
  /** destination port */
  in_port_t dstport;
  /** destination as number */
  uint16_t dstasn;
  /** a buffer of packet */
  char msg[cors::CORS_MAX_MSG_SIZE];
  /** vector of all local interfaces */
  std::vector<NetInterface> ifvec;
  // make sendto & recvfrom atomic
  // pthread_mutex_t cors_lock;
  /** last time of an inquiry being sent out */
  time_t last_query_time;

  // pthread_t cycle_tid;
};

#endif

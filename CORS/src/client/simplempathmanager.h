// Copyright (C) 2007 Hui Zhang <zhang.w.hui@gmail.scom>
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

#ifndef __CLIENT_SIMPLEMPATHMANAGER_H
#define __CLIENT_SIMPLEMPATHMANAGER_H

#include <list>
#include <pthread.h>
#include <math.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include "multipathmanager.h"
#include "relayselector.h"
#include "../utils/debug.h"

using namespace cors;
using namespace std;

#define TRUE			1
#define FALSE			0
#define VERSION		3

#define ZERO			0
#define ONE				1
#define TWO				2
#define THREE			3
#define FOUR			4
#define FIVE			5
#define SIX				6
#define SEVEN			7

#define IMPORTANCE 0
#define NORMAL 1

#define PORT_OFFSET 3
#define ADDR_OFFSET 5

// #define ENABLE_LOSS
#ifdef ENABLE_LOSS
#define LOSSRATE 400
#endif

/** A simple implementation of MultipathManager
 */
class SimpleMpathManager : public MultipathManager {
public:
  /** a list of relay node, copy from RelaySelector */
  list<RelayNode> relaylist; //copy from relay selection module
  /** constructor
   * \param sk pointer to a CorsSocket object
   */
  SimpleMpathManager(CorsSocket * sk) : MultipathManager(sk) {
    srand(time(NULL)); 
  }
  
  virtual int add_relay(RelayNode & r){}
  /** send a packet
   * \param buff pointer to the buffer of a packet
   * \param nbytes length of packet
   * \param dst destination socket address
   * \param ft packet Feature
   * \param sk descriptor of socket
   */
  virtual int send_packet(char * buff, int nbytes, const sockaddr_in & dst, const Feature & ft, int sk);
  /** parse a data packet (packet with a TYPE of TYPE_DATA)
   * strip the CORS headers and return payload to upper layer
   * \param buff pointer to a buffer where payload of the packet to be stored
   * \param bufsize filled with the length of payload
   * \param saddr filled with the source ip address
   * \param sport filled with the source port
   * \param peeraddr filled with address of where the packet come from (relay node or source node)
   * \param sk descriptor of socket
   * \param flag filled with a mark indicating direct or indirect path (if flag is not NULL)
   */
  virtual char * parse_data_packet(char * buff, int & bufsize, in_addr_t & saddr, in_port_t & sport, sockaddr_in &peeraddr, int sk, int * flag);
  /** process a path performance statistics packet (TYPE_STAT)
   * do nothing here
   */
  virtual void process_path_statistics(char * msg, int nbytes, sockaddr_in & peer, int sk){};
  virtual bool get_requirement(Requirement & rq);
  virtual void release_resources(int sk);
  
private:
  vector<RelayNode> goodrelays; // select relay nodes from relaylist
};

#endif

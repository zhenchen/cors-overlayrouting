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

#include <iostream>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "corsproxy.h"
#include "../utils/debug.h"
#include "../utils/unpifi.h"
#include "../utils/utilfunc.h"
#include "../common/corsprotocol.h"
#include "eventqueue.h"

using namespace std;
using namespace cors;

CorsProxy::CorsProxy(NeighborMaintainer * n, PacketForwarder * f, 
    RelayAdvisor * r, uint16_t minp, uint16_t maxp)
: nbmtr(n), fwdr(f), rldvr(r)
{
  daemonsk = socket(AF_INET, SOCK_DGRAM, 0);

  daemonaddr.sin_family = AF_INET;
  daemonaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  uint16_t port;
  for (port = minp; port < maxp; ++port) {
    daemonaddr.sin_port = htons(port);
    if (bind(daemonsk, (sockaddr *) & daemonaddr, sizeof(sockaddr)) == 0)
      break;
  }
  if (port >= maxp) {
    DEBUG_PRINT(1, "creating daemon socket failed!\n");
    exit(-1);
  }
  
  DEBUG_PRINT(4, "CorsProxy::constructor(%s)\n", sock2str(daemonsk).c_str());

}

void CorsProxy::start()
{
  time_t probe_intrv = nbmtr->get_probe_interval();

  char msg[CORS_MAX_MSG_SIZE];
  fd_set rset;
  sockaddr_in peeraddr;
  socklen_t socklen;
  FD_ZERO(&rset);

  timeval tv = { 0, 10 };

  int nready;
  int maxfd = daemonsk + 1;
  nbmtr->start(daemonsk);
  rldvr->start(daemonsk, nbmtr);
  EventQueue *q = EventQueue::Instance();
  while (true) {
    q->run();
    tv.tv_sec = q->get_idle_time();
    FD_SET(daemonsk, &rset);
    nready = select(maxfd, &rset, NULL, NULL, &tv);
    if (nready < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        DEBUG_PRINT(1, "select in corsproxy failed\n");
        exit(-1);
      }
    } else if (nready > 0) {
      dispatch(rset);
    }
  }
}

////////////////////////////////
// private functions
////////////////////////////////

// dispatch socket events
void CorsProxy::dispatch(fd_set & rset)
{
  if (FD_ISSET(daemonsk, &rset)) {
    sockaddr_in peeraddr;
    socklen_t socklen = sizeof(sockaddr_in);
    int len = recvfrom(daemonsk, msg, sizeof(msg) / sizeof(char), 0, (sockaddr *) & peeraddr, &socklen);
    if (len > 0) {
      switch (msg[0]) {
      case TYPE_PROBE_PING:
        nbmtr->process_probe_ping(msg, len, peeraddr, daemonsk);
        break;
      case TYPE_PROBE_PONG:
        nbmtr->process_probe_pong(msg, len, peeraddr, daemonsk);
        break;
      case TYPE_FW_PROBE:
        fwdr->process_probe(msg, len, peeraddr, daemonsk);
        break;
      case TYPE_BW_RESERVE:
        fwdr->process_bw_reserve(msg, len, peeraddr, daemonsk);
        break;
      case TYPE_BW_RELEASE:
        fwdr->process_bw_release(msg, len, peeraddr, daemonsk);
        break;
      case TYPE_DATA:
        fwdr->process_data_packet(msg, len, peeraddr, daemonsk);
        break;
      case TYPE_RLY_INQUIRY:
        rldvr->process_rl_inquiry(msg, len, peeraddr, daemonsk, nbmtr);
        break;
      case TYPE_RLY_FILTER:
        rldvr->process_rl_filter(msg, len, peeraddr, daemonsk, nbmtr);
        break;
      case TYPE_RLY_REPORT:
        rldvr->process_rl_report(msg, len, peeraddr, daemonsk);
        break;
      }
    }
  }
}

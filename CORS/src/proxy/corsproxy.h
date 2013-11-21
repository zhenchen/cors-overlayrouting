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

#ifndef __PROXY_CORSPROXY_H
#define __PROXY_CORSPROXY_H

#include "../utils/types.h"
#include "../common/corsprotocol.h"
#include "neighbormaintainer.h"
#include "packetforwarder.h"
#include "relayadvisor.h"

/** Defination of proxy module 
 */
class CorsProxy {
 public:
  /** constructor
   * \param n pointer to an object derived from NeighborMaintainer
   * \param f pointer to an object derived from PacketForwarder
   * \param r pointer to an object derived from RelayAdvisor
   * \param minp the min port number
   * \param maxp the max port number \n the actual port number p is min <= p < max 
   */
  CorsProxy(NeighborMaintainer * n = NULL,
            PacketForwarder * f = NULL, 
            RelayAdvisor * r = NULL, 
            uint16_t minp = 4097, 
            uint16_t maxp = 8191);

  /** start the main loop routine of CorsProxy
   */
  void start();

 private:
  void dispatch(fd_set & rset);

  NeighborMaintainer *nbmtr;
  PacketForwarder *fwdr;
  RelayAdvisor *rldvr;
  sockaddr_in daemonaddr;
  int daemonsk;
  char msg[cors::CORS_MAX_MSG_SIZE];
};

#endif

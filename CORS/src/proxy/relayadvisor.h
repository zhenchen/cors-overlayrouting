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

#ifndef __PROXY_RELAYADVISOR_H
#define __PROXY_RELAYADVISOR_H

#include <netinet/in.h>
#include "neighbormaintainer.h"

class RelayAdvisor {
 public:
  virtual void start(int sk, NeighborMaintainer * nbmtr) = 0;
  virtual void update(int sk, NeighborMaintainer * nbmtr) = 0;
  virtual time_t get_update_interval() = 0;

  virtual void process_rl_inquiry(char * msg, int len, sockaddr_in & peeraddr, int sk, NeighborMaintainer *nbmtr) = 0;
  virtual void process_rl_report(char * msg, int len, sockaddr_in & peeraddr, int sk) = 0;
  virtual void process_rl_filter(char * msg, int len, sockaddr_in & peeraddr, int sk, NeighborMaintainer *nbmtr) = 0;
};

#endif
